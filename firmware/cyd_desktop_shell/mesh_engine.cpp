// See include/mesh_engine.h. The transform follows is01_mesh.inc step by
// step, in single precision (the ESP32 has a float unit but no double one).

#include "mesh_engine.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace cyd::desktop::mesh {

namespace {

struct Point {
    int16_t x, y;
    float depth;
};

struct Face {
    Point points[4];
    float depth;
    uint16_t color;
    uint16_t order;
    uint8_t count;
};

uint16_t read_u16(const uint8_t *bytes) { return static_cast<uint16_t>(bytes[0] | (bytes[1] << 8)); }
int16_t read_i16(const uint8_t *bytes) { return static_cast<int16_t>(read_u16(bytes)); }

bool sane(float value) { return std::isfinite(value) && std::fabs(value) <= 1000000.0f; }

int16_t coordinate(float value) {
    if (value < -16383.0f) return -16383;
    if (value > 16383.0f) return 16383;
    return static_cast<int16_t>(value);
}

}  // namespace

int project(const Camera &camera, const Instance *instances, int instance_count,
            Triangle *out, int capacity, int *dropped) {
    if (dropped != nullptr) *dropped = 0;
    if (instance_count < 0 || instance_count > kMaxInstances) return kTooMany;
    for (float value : {camera.x, camera.y, camera.z, camera.yaw, camera.pitch, camera.roll, camera.focal})
        if (!sane(value)) return kBadTransform;
    if (camera.focal <= 0.0f) return kBadTransform;

    // Validate every instance before allocating anything.
    int scene_faces = 0;
    for (int index = 0; index < instance_count; ++index) {
        const Instance &instance = instances[index];
        for (float value : {instance.x, instance.y, instance.z, instance.yaw, instance.pitch, instance.roll,
                            instance.scale})
            if (!sane(value)) return kBadTransform;
        if (instance.scale <= 0.0f) return kBadTransform;
        if (instance.data == nullptr || instance.size < 4) return kBadMesh;
        const unsigned vertices = read_u16(instance.data), faces = read_u16(instance.data + 2);
        if (vertices == 0 || vertices > kMaxVertices || faces == 0 || faces > kMaxFaces ||
            instance.size != 4 + vertices * 6 + faces * 7)
            return kBadMesh;
        const uint8_t *face = instance.data + 4 + vertices * 6;
        for (unsigned f = 0; f < faces; ++f, face += 7) {
            const unsigned corners = face[2];
            if (corners != 3 && corners != 4) return kBadMesh;
            for (unsigned k = 0; k < corners; ++k)
                if (face[3 + k] >= vertices) return kBadMesh;
        }
        scene_faces += static_cast<int>(faces);
        if (scene_faces > kMaxSceneFaces) return kTooMany;
    }

    Face *faces = static_cast<Face *>(std::malloc(sizeof(Face) * static_cast<std::size_t>(std::max(1, scene_faces))));
    Face **order = static_cast<Face **>(std::malloc(sizeof(Face *) * static_cast<std::size_t>(std::max(1, scene_faces))));
    Point *points = static_cast<Point *>(std::malloc(sizeof(Point) * kMaxVertices));
    if (faces == nullptr || order == nullptr || points == nullptr) {
        std::free(faces);
        std::free(order);
        std::free(points);
        return kNoMemory;
    }

    const float yaw = -camera.yaw, pitch = -camera.pitch, roll = -camera.roll;
    const float yc = std::cos(yaw), ys = std::sin(yaw), pc = std::cos(pitch), ps = std::sin(pitch);
    const float rc = std::cos(roll), rs = std::sin(roll);
    int visible = 0;
    for (int index = 0; index < instance_count; ++index) {
        const Instance &instance = instances[index];
        const uint8_t *raw = instance.data;
        const unsigned vertices = read_u16(raw), count = read_u16(raw + 2);
        const float ox = instance.x - camera.x, oy = instance.y - camera.y, oz = instance.z - camera.z;
        const float oyc = std::cos(instance.yaw), oys = std::sin(instance.yaw);
        const float opc = std::cos(instance.pitch), ops = std::sin(instance.pitch);
        const float orc = std::cos(instance.roll), ors = std::sin(instance.roll);
        raw += 4;
        for (unsigned j = 0; j < vertices; ++j, raw += 6) {
            const float x = read_i16(raw) * instance.scale, y = read_i16(raw + 2) * instance.scale;
            const float z = read_i16(raw + 4) * instance.scale;
            const float rx = x * oyc - z * oys, rz = x * oys + z * oyc;
            const float ry = y * opc - rz * ops;
            const float dx = rx * orc - ry * ors + ox;
            const float dy = rx * ors + ry * orc + oy;
            const float dz = y * ops + rz * opc + oz;
            const float tx = dx * yc - dz * ys, tz = dx * ys + dz * yc;
            const float ty = dy * pc - tz * ps;
            const float depth = dy * ps + tz * pc;
            points[j].depth = depth;
            if (depth <= kNearZ) continue;
            const float perspective = camera.focal / depth;
            points[j].x = coordinate(kCentreX + (tx * rc - ty * rs) * perspective);
            points[j].y = coordinate(kCentreY - (tx * rs + ty * rc) * perspective);
        }
        for (unsigned j = 0; j < count; ++j, raw += 7) {
            const unsigned corners = raw[2];
            Face &face = faces[visible];
            bool clipped = false;
            float depth = 0.0f;
            for (unsigned k = 0; k < corners; ++k) {
                const Point &point = points[raw[3 + k]];
                if (point.depth <= kNearZ) {
                    clipped = true;
                    break;
                }
                face.points[k] = point;
                depth += point.depth;
            }
            if (clipped) continue;
            const Point *q = face.points;
            const int64_t cross = static_cast<int64_t>(q[1].x - q[0].x) * (q[2].y - q[0].y) -
                                  static_cast<int64_t>(q[1].y - q[0].y) * (q[2].x - q[0].x);
            if (!camera.wire && cross >= 0) continue;
            face.depth = depth / static_cast<float>(corners);
            face.color = read_u16(raw);
            face.count = static_cast<uint8_t>(corners);
            face.order = static_cast<uint16_t>(visible);
            order[visible] = &face;
            ++visible;
        }
    }

    // Furthest first; equal depths keep their order.
    std::stable_sort(order, order + visible, [](const Face *a, const Face *b) { return a->depth > b->depth; });
    int written = 0, refused = 0;
    auto emit = [&](const Face &face, int b, int c) {
        if (written >= capacity) {
            ++refused;
            return;
        }
        const Point *p = face.points;
        out[written++] = Triangle{p[0].x, p[0].y, p[b].x, p[b].y, p[c].x, p[c].y, face.color, camera.wire};
    };
    for (int index = 0; index < visible; ++index) {
        emit(*order[index], 1, 2);
        if (order[index]->count == 4) emit(*order[index], 2, 3);
    }
    std::free(faces);
    std::free(order);
    std::free(points);
    if (dropped != nullptr) *dropped = refused;
    return written;
}

}  // namespace cyd::desktop::mesh
