// See include/n3d_engine.h. The maths follows native_3d.c (Native3D_MakeTRS,
// MatLookAt, BuildModelPolys, XformPoints) so a scene projects to the same
// polygons; what changed is where they go (scene::Poly, rasterized by band)
// and the memory: models and the vertex scratch are allocated on demand and
// freed by reset(), with no 4096-vertex static array or 960x540 Z buffer.

#include "n3d_engine.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace cyd::desktop::n3d {

namespace {

constexpr int32_t kOne = 1 << 16;
constexpr int32_t kZMax = 65535;

struct Matrix {
    int32_t m[12];  // 3x4 row-major Q16.16: rotation | translation
};

struct Vertex {
    int16_t x, y, z;
};

struct Face {
    uint8_t count;
    uint8_t flags;  // bit 0 emissive, bit 1 additive, bit 2 wire
    uint16_t color;
    int16_t nx, ny, nz;
    uint16_t index[4];
};
static_assert(sizeof(Vertex) == 6, "packed vertex");
static_assert(sizeof(Face) == 18, "packed face");

struct Model {
    Vertex *vertices;
    Face *faces;
    int vertex_count, face_count;
    uint16_t flags;  // bit 0: two-sided
};

struct Transformed {
    int32_t x, y, z;  // camera space Q16.16
    bool visible;     // beyond the near plane
};

struct CameraPoint {
    int32_t x, y, z;
};

Model models[kModelCapacity]{};
Transformed *scratch = nullptr;
int scratch_size = 0;
Matrix view{{kOne, 0, 0, 0, 0, kOne, 0, 0, 0, 0, kOne, 0}};
Projection projection{};
float light_direction[3] = {0, 0, -1};
int ambient = 120;
Fog fog{};

uint16_t read_u16(const uint8_t *bytes) { return static_cast<uint16_t>(bytes[0] | (bytes[1] << 8)); }
int32_t read_i32(const uint8_t *bytes) {
    return static_cast<int32_t>(static_cast<uint32_t>(bytes[0]) | (static_cast<uint32_t>(bytes[1]) << 8) |
                                (static_cast<uint32_t>(bytes[2]) << 16) | (static_cast<uint32_t>(bytes[3]) << 24));
}

// native_3d.c's 4096-entry table, computed on demand instead of stored.
int32_t fx_sin(uint16_t angle) {
    const int index = (angle >> 4) & 4095;
    return static_cast<int32_t>(std::sin(2.0 * 3.14159265358979323846 * index / 4096) * kOne);
}
int32_t fx_cos(uint16_t angle) { return fx_sin(static_cast<uint16_t>(angle + 0x4000)); }
int32_t fx_mul(int32_t a, int32_t b) { return static_cast<int32_t>((static_cast<int64_t>(a) * b) >> 16); }

Matrix trs(uint16_t rx, uint16_t ry, uint16_t rz, int32_t tx, int32_t ty, int32_t tz, uint16_t scale_q8) {
    const int32_t sx = fx_sin(rx), cx = fx_cos(rx), sy = fx_sin(ry), cy = fx_cos(ry);
    const int32_t sz = fx_sin(rz), cz = fx_cos(rz);
    const int32_t scale = static_cast<int32_t>(scale_q8 ? scale_q8 : 256) << 8;
    Matrix d{};
    // R = Rz * Ry * Rx
    d.m[0] = fx_mul(cz, cy);
    d.m[1] = fx_mul(fx_mul(cz, sy), sx) - fx_mul(sz, cx);
    d.m[2] = fx_mul(fx_mul(cz, sy), cx) + fx_mul(sz, sx);
    d.m[3] = tx;
    d.m[4] = fx_mul(sz, cy);
    d.m[5] = fx_mul(fx_mul(sz, sy), sx) + fx_mul(cz, cx);
    d.m[6] = fx_mul(fx_mul(sz, sy), cx) - fx_mul(cz, sx);
    d.m[7] = ty;
    d.m[8] = -sy;
    d.m[9] = fx_mul(cy, sx);
    d.m[10] = fx_mul(cy, cx);
    d.m[11] = tz;
    if (scale != kOne) {
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) d.m[i * 4 + j] = fx_mul(d.m[i * 4 + j], scale);
    }
    return d;
}

// a then b: the result applies b first.
Matrix multiply(const Matrix &a, const Matrix &b) {
    Matrix r{};
    for (int i = 0; i < 3; ++i) {
        const int32_t a0 = a.m[i * 4], a1 = a.m[i * 4 + 1], a2 = a.m[i * 4 + 2];
        r.m[i * 4 + 0] = fx_mul(a0, b.m[0]) + fx_mul(a1, b.m[4]) + fx_mul(a2, b.m[8]);
        r.m[i * 4 + 1] = fx_mul(a0, b.m[1]) + fx_mul(a1, b.m[5]) + fx_mul(a2, b.m[9]);
        r.m[i * 4 + 2] = fx_mul(a0, b.m[2]) + fx_mul(a1, b.m[6]) + fx_mul(a2, b.m[10]);
        r.m[i * 4 + 3] = fx_mul(a0, b.m[3]) + fx_mul(a1, b.m[7]) + fx_mul(a2, b.m[11]) + a.m[i * 4 + 3];
    }
    return r;
}

void normalise(double *v) {
    double length = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (length < 1e-9) length = 1.0;
    v[0] /= length;
    v[1] /= length;
    v[2] /= length;
}

Matrix look_at(const int32_t *eye_at_up) {
    double eye[3], at[3], up[3], f[3], r[3], u[3];
    for (int i = 0; i < 3; ++i) {
        eye[i] = static_cast<double>(eye_at_up[i]) / kOne;
        at[i] = static_cast<double>(eye_at_up[3 + i]) / kOne;
        up[i] = static_cast<double>(eye_at_up[6 + i]) / kOne;
    }
    for (int i = 0; i < 3; ++i) f[i] = at[i] - eye[i];
    normalise(f);
    r[0] = up[1] * f[2] - up[2] * f[1];
    r[1] = up[2] * f[0] - up[0] * f[2];
    r[2] = up[0] * f[1] - up[1] * f[0];
    normalise(r);
    u[0] = f[1] * r[2] - f[2] * r[1];
    u[1] = f[2] * r[0] - f[0] * r[2];
    u[2] = f[0] * r[1] - f[1] * r[0];
    Matrix d{};
    const double *rows[3] = {r, u, f};
    for (int i = 0; i < 3; ++i) {
        const double *row = rows[i];
        d.m[i * 4 + 0] = static_cast<int32_t>(row[0] * kOne);
        d.m[i * 4 + 1] = static_cast<int32_t>(row[1] * kOne);
        d.m[i * 4 + 2] = static_cast<int32_t>(row[2] * kOne);
        d.m[i * 4 + 3] = static_cast<int32_t>(-(row[0] * eye[0] + row[1] * eye[1] + row[2] * eye[2]) * kOne);
    }
    return d;
}

int32_t transform_axis(const int32_t *row, int x, int y, int z) {
    int64_t value = static_cast<int64_t>(row[0]) * x + static_cast<int64_t>(row[1]) * y +
                    static_cast<int64_t>(row[2]) * z + row[3];
    value = std::max<int64_t>(-INT32_MAX, std::min<int64_t>(INT32_MAX, value));
    return static_cast<int32_t>(value);
}

uint16_t depth_key(int32_t z) {
    if (z <= projection.near_q16) return kZMax;
    const int64_t key = (static_cast<int64_t>(kZMax) * projection.near_q16) / z;
    return static_cast<uint16_t>(std::max<int64_t>(1, key));
}

void project(int32_t x, int32_t y, int32_t z, int32_t *sx, int32_t *sy) {
    *sx = projection.centre_x + static_cast<int32_t>((static_cast<int64_t>(x) * projection.focal_q8) /
                                                     (static_cast<int64_t>(z) * 256));
    *sy = projection.centre_y + static_cast<int32_t>((static_cast<int64_t>(y) * projection.focal_q8) /
                                                     (static_cast<int64_t>(z) * 256));
}

uint16_t shade(uint16_t color, int light) {
    const uint32_t l = static_cast<uint32_t>(light);
    const uint32_t r = (((color >> 11) & 0x1f) * l * 32897u) >> 23;
    const uint32_t g = (((color >> 5) & 0x3f) * l * 32897u) >> 23;
    const uint32_t b = ((color & 0x1f) * l * 32897u) >> 23;
    return static_cast<uint16_t>((r << 11) | (g << 5) | b);
}

uint16_t fogged(uint16_t color, int amount) {
    const uint32_t f = static_cast<uint32_t>(amount), keep = 255u - f;
    const uint32_t r = ((((color >> 11) & 0x1f) * keep + ((fog.color >> 11) & 0x1f) * f) * 32897u) >> 23;
    const uint32_t g = ((((color >> 5) & 0x3f) * keep + ((fog.color >> 5) & 0x3f) * f) * 32897u) >> 23;
    const uint32_t b = (((color & 0x1f) * keep + (fog.color & 0x1f) * f) * 32897u) >> 23;
    return static_cast<uint16_t>((r << 11) | (g << 5) | b);
}

// Clips a convex polygon to camera z >= near; at most one more vertex.
int clip_near(const CameraPoint *in, int count, CameraPoint *out) {
    int written = 0;
    for (int i = 0; i < count && written < scene::kPolyVertices; ++i) {
        const CameraPoint &a = in[i];
        const CameraPoint &b = in[(i + 1) % count];
        const bool a_in = a.z >= projection.near_q16, b_in = b.z >= projection.near_q16;
        if (a_in) out[written++] = a;
        if (a_in != b_in && written < scene::kPolyVertices) {
            const int64_t dz = static_cast<int64_t>(b.z) - a.z;
            if (dz == 0) continue;
            const int64_t t = ((static_cast<int64_t>(projection.near_q16) - a.z) << 16) / dz;
            out[written].x = a.x + static_cast<int32_t>((static_cast<int64_t>(b.x - a.x) * t) >> 16);
            out[written].y = a.y + static_cast<int32_t>((static_cast<int64_t>(b.y - a.y) * t) >> 16);
            out[written].z = projection.near_q16;
            ++written;
        }
    }
    return written;
}

void free_model(Model &model) {
    std::free(model.vertices);
    std::free(model.faces);
    model = Model{};
}

}  // namespace

Projection default_projection() { return Projection{160, 127, 217 * 240, 4 * kOne}; }
Light default_light() { return Light{-5000, -12000, -9000, 120}; }
Fog default_fog() { return Fog{0x0001, 600 * kOne, 2400 * kOne}; }

void reset() {
    for (Model &model : models) free_model(model);
    std::free(scratch);
    scratch = nullptr;
    scratch_size = 0;
    view = Matrix{{kOne, 0, 0, 0, 0, kOne, 0, 0, 0, 0, kOne, 0}};
    set_projection(default_projection());
    set_light(default_light());
    set_fog(default_fog());
}

int load_model(int slot, const uint8_t *data, std::size_t size) {
    if (slot < 0 || slot >= kModelCapacity) return kBadSlot;
    if (data == nullptr || size < 8) return kBadModel;
    const int vertex_count = read_u16(data), face_count = read_u16(data + 2);
    if (vertex_count < 1 || vertex_count > kMaxModelVertices || face_count < 1 || face_count > kMaxModelFaces ||
        size != 8 + static_cast<std::size_t>(vertex_count) * sizeof(Vertex) +
                    static_cast<std::size_t>(face_count) * sizeof(Face))
        return kBadModel;
    const uint8_t *face_bytes = data + 8 + static_cast<std::size_t>(vertex_count) * sizeof(Vertex);
    for (int i = 0; i < face_count; ++i) {
        const uint8_t *face = face_bytes + static_cast<std::size_t>(i) * sizeof(Face);
        const int corners = face[0];
        if (corners != 3 && corners != 4) return kBadModel;
        for (int k = 0; k < corners; ++k)
            if (read_u16(face + 10 + k * 2) >= vertex_count) return kBadModel;
    }
    Model loaded{};
    loaded.vertices = static_cast<Vertex *>(std::malloc(sizeof(Vertex) * vertex_count));
    loaded.faces = static_cast<Face *>(std::malloc(sizeof(Face) * face_count));
    if (loaded.vertices == nullptr || loaded.faces == nullptr) {
        free_model(loaded);
        return kNoMemory;
    }
    // Little-endian packed records and a little-endian target: a plain copy.
    std::memcpy(loaded.vertices, data + 8, sizeof(Vertex) * vertex_count);
    std::memcpy(loaded.faces, face_bytes, sizeof(Face) * face_count);
    loaded.vertex_count = vertex_count;
    loaded.face_count = face_count;
    loaded.flags = read_u16(data + 6);
    free_model(models[slot]);
    models[slot] = loaded;
    return kOk;
}

bool model_loaded(int slot) { return slot >= 0 && slot < kModelCapacity && models[slot].vertices != nullptr; }

void set_view(const uint8_t camera[kCameraBytes], int32_t *matrix_out) {
    int32_t eye_at_up[9] = {0, 0, 0, 0, 0, 0, 0, -kOne, 0};
    for (int i = 0; i < 6; ++i) eye_at_up[i] = read_i32(camera + i * 4);
    const Matrix rotation = trs(read_u16(camera + 24), read_u16(camera + 26), read_u16(camera + 28), 0, 0, 0, 256);
    view = multiply(rotation, look_at(eye_at_up));
    if (matrix_out != nullptr) std::memcpy(matrix_out, view.m, sizeof(view.m));
}

void set_projection(const Projection &value) {
    projection = value;
    if (projection.focal_q8 == 0) projection.focal_q8 = 256;
    if (projection.near_q16 <= 0) projection.near_q16 = 1;
}

void set_light(const Light &value) {
    double v[3] = {value.x / 16384.0, value.y / 16384.0, value.z / 16384.0};
    if (v[0] == 0.0 && v[1] == 0.0 && v[2] == 0.0) v[2] = -1.0;
    normalise(v);
    for (int i = 0; i < 3; ++i) light_direction[i] = static_cast<float>(v[i]);
    ambient = value.ambient;
}

void set_fog(const Fog &value) { fog = value; }

int build_item(const uint8_t *item, bool wire, scene::Poly *out, int capacity) {
    const uint16_t slot = read_u16(item), flags = read_u16(item + 2);
    if (!(flags & kEnable) || !model_loaded(slot) || capacity <= 0) return 0;
    const Model &model = models[slot];
    const uint16_t color_mod = read_u16(item + 24);
    const uint8_t alpha = item[26] ? item[26] : 255;
    uint8_t poly_flags = (wire || (flags & kWire)) ? scene::kPolyWire : 0;
    if (flags & kAdd) poly_flags |= scene::kPolyAdd;
    const Matrix pose = trs(read_u16(item + 4), read_u16(item + 6), read_u16(item + 8), read_i32(item + 12),
                            read_i32(item + 16), read_i32(item + 20), read_u16(item + 10));
    const Matrix modelview = multiply(view, pose);

    if (scratch_size < model.vertex_count) {
        auto *grown = static_cast<Transformed *>(std::realloc(scratch, sizeof(Transformed) * model.vertex_count));
        if (grown == nullptr) return 0;
        scratch = grown;
        scratch_size = model.vertex_count;
    }
    for (int i = 0; i < model.vertex_count; ++i) {
        const Vertex &v = model.vertices[i];
        Transformed &t = scratch[i];
        t.x = transform_axis(&modelview.m[0], v.x, v.y, v.z);
        t.y = transform_axis(&modelview.m[4], v.x, v.y, v.z);
        t.z = transform_axis(&modelview.m[8], v.x, v.y, v.z);
        t.visible = t.z >= projection.near_q16;
    }

    int written = 0;
    for (int i = 0; i < model.face_count && written < capacity; ++i) {
        const Face &face = model.faces[i];
        CameraPoint corners[scene::kPolyVertices];
        int visible = 0;
        for (int k = 0; k < face.count; ++k)
            if (scratch[face.index[k]].visible) ++visible;
        if (visible == 0) continue;
        int count = face.count;
        if (visible == face.count) {
            for (int k = 0; k < count; ++k) {
                const Transformed &t = scratch[face.index[k]];
                corners[k] = CameraPoint{t.x, t.y, t.z};
            }
        } else {
            CameraPoint source[4];
            for (int k = 0; k < face.count; ++k) {
                const Transformed &t = scratch[face.index[k]];
                source[k] = CameraPoint{t.x, t.y, t.z};
            }
            count = clip_near(source, face.count, corners);
            if (count < 3) continue;
        }

        scene::Poly &poly = out[written];
        poly.count = static_cast<uint8_t>(count);
        int32_t z_sum = 0;
        for (int k = 0; k < count; ++k) {
            project(corners[k].x, corners[k].y, corners[k].z, &poly.x[k], &poly.y[k]);
            poly.z[k] = depth_key(corners[k].z);
            z_sum += corners[k].z >> 8;
        }
        const int64_t area2 = static_cast<int64_t>(poly.x[1] - poly.x[0]) * (poly.y[2] - poly.y[0]) -
                              static_cast<int64_t>(poly.y[1] - poly.y[0]) * (poly.x[2] - poly.x[0]);
        if (area2 == 0) continue;
        if (!(poly_flags & scene::kPolyWire) && !(model.flags & 0x01) && area2 > 0) continue;

        int light;
        if (face.flags & 0x01) {
            light = 255;
        } else {
            const float ux = static_cast<float>(corners[1].x - corners[0].x);
            const float uy = static_cast<float>(corners[1].y - corners[0].y);
            const float uz = static_cast<float>(corners[1].z - corners[0].z);
            const float vx = static_cast<float>(corners[2].x - corners[0].x);
            const float vy = static_cast<float>(corners[2].y - corners[0].y);
            const float vz = static_cast<float>(corners[2].z - corners[0].z);
            const float nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
            const float length2 = nx * nx + ny * ny + nz * nz;
            if (length2 < 1e-6f) {
                light = ambient;
            } else {
                float dot = nx * light_direction[0] + ny * light_direction[1] + nz * light_direction[2];
                if (nz > 0.0f) dot = -dot;  // the visible side faces the camera (-z)
                const float lit = -dot / std::sqrt(length2);
                light = lit <= 0.0f ? ambient : std::min(255, ambient + static_cast<int>((255 - ambient) * lit));
            }
        }
        uint16_t color = color_mod == 0xffff ? face.color : color_mod;
        if (light != 255) color = shade(color, light);
        if (fog.end_q16 > fog.start_q16) {
            const int32_t average = (z_sum / count) << 8;
            if (average > fog.start_q16) {
                const int64_t t = std::min<int64_t>(
                    255, (static_cast<int64_t>(average - fog.start_q16) * 255) / (fog.end_q16 - fog.start_q16));
                if (t > 0) color = fogged(color, static_cast<int>(t));
            }
        }
        poly.color = color;
        poly.alpha = alpha;
        poly.flags = static_cast<uint8_t>(poly_flags | ((face.flags & 0x02) ? scene::kPolyAdd : 0) |
                                          ((face.flags & 0x04) ? scene::kPolyWire : 0));
        if ((poly.flags & scene::kPolyAdd) || alpha < 255) poly.flags |= scene::kPolyNoZWrite;
        ++written;
    }
    return written;
}

int project_points(const uint8_t *points, int count, uint8_t *out) {
    int visible = 0;
    for (int i = 0; i < count; ++i) {
        const uint8_t *in = points + i * 12;
        const int64_t wx = read_i32(in), wy = read_i32(in + 4), wz = read_i32(in + 8);
        const int32_t *m = view.m;
        const int32_t x = static_cast<int32_t>(((m[0] * wx + m[1] * wy + m[2] * wz) >> 16) + m[3]);
        const int32_t y = static_cast<int32_t>(((m[4] * wx + m[5] * wy + m[6] * wz) >> 16) + m[7]);
        const int32_t z = static_cast<int32_t>(((m[8] * wx + m[9] * wy + m[10] * wz) >> 16) + m[11]);
        uint8_t *record = out + i * 6;
        int16_t sx = 0, sy = 0;
        uint16_t depth = 0;
        if (z >= projection.near_q16) {
            int32_t px, py;
            project(x, y, z, &px, &py);
            sx = static_cast<int16_t>(std::max<int32_t>(-16000, std::min<int32_t>(16000, px)));
            sy = static_cast<int16_t>(std::max<int32_t>(-16000, std::min<int32_t>(16000, py)));
            depth = depth_key(z);
            ++visible;
        }
        record[0] = static_cast<uint8_t>(sx & 0xff);
        record[1] = static_cast<uint8_t>((sx >> 8) & 0xff);
        record[2] = static_cast<uint8_t>(sy & 0xff);
        record[3] = static_cast<uint8_t>((sy >> 8) & 0xff);
        record[4] = static_cast<uint8_t>(depth & 0xff);
        record[5] = static_cast<uint8_t>(depth >> 8);
    }
    return visible;
}

}  // namespace cyd::desktop::n3d
