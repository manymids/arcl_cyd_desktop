#include "phys_engine.h"

#include <math.h>
#include <string.h>

#define PHYS_ITERS 8
#define PHYS_SLOP  0.25f
#define PHYS_PERCENT 0.55f
#define PHYS_MAX_SPEED 900.0f
#define PHYS_REST_VN 14.0f

typedef struct {
    uint8_t active;
    uint8_t shape;
    uint8_t grounded;
    float x, y, z;
    float vx, vy, vz;
    float hx, hy, hz;
    float plane_d;
    float inv_mass;
    float restitution;
    float friction;
} body_t;

static struct {
    int dim;
    float gx, gy, gz;
    body_t bodies[PHYS_CAP];
} world;

/* 最終反復だけ接触を数える。負のときは数えない。 */
static int hit_tally = -1;

typedef char phys_state_size_check[sizeof(phys_state_t) == 40 ? 1 : -1];

static float sanef(float v, float fallback) {
    if (v != v || v > 1.0e6f || v < -1.0e6f) return fallback;
    return v;
}

static float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static int slot_ok(int id) {
    return id >= 0 && id < PHYS_CAP;
}

static int dynamic_body(const body_t *b) {
    return b->active && b->inv_mass > 0.0f;
}

void phys_reset(int dim, float gx, float gy, float gz) {
    memset(&world, 0, sizeof(world));
    world.dim = (dim == 2) ? 2 : 3;
    world.gx = sanef(gx, 0.0f);
    world.gy = sanef(gy, 0.0f);
    world.gz = sanef(gz, 0.0f);
}

int phys_body(int id, int shape,
              float x, float y, float z,
              float hx, float hy, float hz,
              float mass, float restitution, float friction) {
    body_t *b;
    if (!slot_ok(id)) return -1;
    b = &world.bodies[id];
    if (shape == PHYS_SHAPE_NONE) {
        memset(b, 0, sizeof(*b));
        return id;
    }
    if (shape != PHYS_SHAPE_SPHERE && shape != PHYS_SHAPE_BOX && shape != PHYS_SHAPE_PLANE) {
        return -1;
    }
    memset(b, 0, sizeof(*b));
    b->active = 1;
    b->shape = (uint8_t)shape;
    b->x = sanef(x, 0.0f);
    b->y = sanef(y, 0.0f);
    b->z = sanef(z, 0.0f);
    b->hx = sanef(hx, 1.0f);
    b->hy = sanef(hy, 1.0f);
    b->hz = sanef(hz, 1.0f);
    b->restitution = clampf(sanef(restitution, 0.2f), 0.0f, 1.0f);
    b->friction = clampf(sanef(friction, 0.4f), 0.0f, 2.0f);
    if (world.dim == 2) {
        b->z = 0.0f;
    }
    if (shape == PHYS_SHAPE_SPHERE) {
        if (b->hx < 0.5f) b->hx = 0.5f;
        b->hy = 0.0f;
        b->hz = 0.0f;
    } else if (shape == PHYS_SHAPE_BOX) {
        if (b->hx < 0.5f) b->hx = 0.5f;
        if (b->hy < 0.5f) b->hy = 0.5f;
        if (b->hz < 0.5f) b->hz = 0.5f;
    } else {
        float len = sqrtf(b->hx * b->hx + b->hy * b->hy + b->hz * b->hz);
        if (len < 1.0e-4f) {
            b->hx = 0.0f;
            b->hy = 1.0f;
            b->hz = 0.0f;
            len = 1.0f;
        }
        b->hx /= len;
        b->hy /= len;
        b->hz /= len;
        b->plane_d = b->hx * b->x + b->hy * b->y + b->hz * b->z;
        b->inv_mass = 0.0f;
        return id;
    }
    if (mass > 0.0f && mass < 1.0e5f) {
        b->inv_mass = 1.0f / mass;
    }
    return id;
}

int phys_velocity(int id, float vx, float vy, float vz) {
    body_t *b;
    if (!slot_ok(id) || !dynamic_body(&world.bodies[id])) return 0;
    b = &world.bodies[id];
    b->vx = sanef(vx, 0.0f);
    b->vy = sanef(vy, 0.0f);
    b->vz = world.dim == 2 ? 0.0f : sanef(vz, 0.0f);
    return 1;
}

static void clamp_speed(body_t *b) {
    float s2 = b->vx * b->vx + b->vy * b->vy + b->vz * b->vz;
    float limit = PHYS_MAX_SPEED * PHYS_MAX_SPEED;
    if (s2 > limit) {
        float k = PHYS_MAX_SPEED / sqrtf(s2);
        b->vx *= k;
        b->vy *= k;
        b->vz *= k;
    }
}

static void resolve(body_t *a, body_t *b, float nx, float ny, float nz, float pen, int bounce) {
    float ima, imb, im;
    float rvx, rvy, rvz, vn, e, j;
    if (pen <= 0.0f) return;
    if (hit_tally >= 0 && pen > 0.05f) hit_tally++;
    ima = a->inv_mass;
    imb = b->inv_mass;
    im = ima + imb;
    if (im <= 0.0f) return;
    if (ima == 0.0f) b->grounded = 1;
    if (imb == 0.0f) a->grounded = 1;
    rvx = b->vx - a->vx;
    rvy = b->vy - a->vy;
    rvz = b->vz - a->vz;
    vn = rvx * nx + rvy * ny + rvz * nz;
    if (vn < 0.0f) {
        e = 0.0f;
        if (bounce) {
            e = a->restitution < b->restitution ? a->restitution : b->restitution;
            if ((ima == 0.0f || imb == 0.0f) && -vn < PHYS_REST_VN) e = 0.0f;
        }
        j = -(1.0f + e) * vn / im;
        a->vx -= j * nx * ima;
        a->vy -= j * ny * ima;
        a->vz -= j * nz * ima;
        b->vx += j * nx * imb;
        b->vy += j * ny * imb;
        b->vz += j * nz * imb;
        {
            float tx = rvx - vn * nx;
            float ty = rvy - vn * ny;
            float tz = rvz - vn * nz;
            float tl2 = tx * tx + ty * ty + tz * tz;
            if (tl2 > 1.0e-8f && j > 0.0f) {
                float tl = sqrtf(tl2);
                float vt, mu, jt, maxf;
                tx /= tl;
                ty /= tl;
                tz /= tl;
                vt = rvx * tx + rvy * ty + rvz * tz;
                mu = a->friction < b->friction ? a->friction : b->friction;
                jt = -vt / im;
                maxf = mu * j;
                if (jt > maxf) jt = maxf;
                if (jt < -maxf) jt = -maxf;
                a->vx -= jt * tx * ima;
                a->vy -= jt * ty * ima;
                a->vz -= jt * tz * ima;
                b->vx += jt * tx * imb;
                b->vy += jt * ty * imb;
                b->vz += jt * tz * imb;
            }
        }
    }
    if (pen > PHYS_SLOP) {
        float corr = (pen - PHYS_SLOP) * PHYS_PERCENT;
        if (corr > 48.0f) corr = 48.0f;
        corr /= im;
        a->x -= nx * corr * ima;
        a->y -= ny * corr * ima;
        a->z -= nz * corr * ima;
        b->x += nx * corr * imb;
        b->y += ny * corr * imb;
        b->z += nz * corr * imb;
        if (world.dim == 2) {
            a->z = 0.0f;
            b->z = 0.0f;
        }
    }
}

static void collide_spheres(body_t *a, body_t *b, int bounce) {
    float dx = b->x - a->x;
    float dy = b->y - a->y;
    float dz = (world.dim == 2) ? 0.0f : (b->z - a->z);
    float rad = a->hx + b->hx;
    float d2 = dx * dx + dy * dy + dz * dz;
    float dist, nx, ny, nz, pen;
    if (d2 >= rad * rad) return;
    dist = sqrtf(d2);
    if (dist < 1.0e-5f) {
        nx = 1.0f;
        ny = 0.0f;
        nz = 0.0f;
        pen = rad;
    } else {
        nx = dx / dist;
        ny = dy / dist;
        nz = dz / dist;
        pen = rad - dist;
    }
    resolve(a, b, nx, ny, nz, pen, bounce);
}

static void collide_boxes(body_t *a, body_t *b, int bounce) {
    float dx = b->x - a->x;
    float dy = b->y - a->y;
    float dz = b->z - a->z;
    float px = (a->hx + b->hx) - fabsf(dx);
    float py = (a->hy + b->hy) - fabsf(dy);
    float pz = (world.dim == 2) ? 1.0e9f : ((a->hz + b->hz) - fabsf(dz));
    float pen, nx, ny, nz;
    if (px <= 0.0f || py <= 0.0f || pz <= 0.0f) return;
    nx = ny = nz = 0.0f;
    if (px < py && px < pz) {
        pen = px;
        nx = (dx < 0.0f) ? -1.0f : 1.0f;
    } else if (py < pz) {
        pen = py;
        ny = (dy < 0.0f) ? -1.0f : 1.0f;
    } else {
        pen = pz;
        nz = (dz < 0.0f) ? -1.0f : 1.0f;
    }
    resolve(a, b, nx, ny, nz, pen, bounce);
}

static void collide_sphere_box(body_t *s, body_t *box, int bounce) {
    float minx = box->x - box->hx, maxx = box->x + box->hx;
    float miny = box->y - box->hy, maxy = box->y + box->hy;
    float minz = box->z - box->hz, maxz = box->z + box->hz;
    float cx = clampf(s->x, minx, maxx);
    float cy = clampf(s->y, miny, maxy);
    float cz = (world.dim == 2) ? s->z : clampf(s->z, minz, maxz);
    float dx = s->x - cx;
    float dy = s->y - cy;
    float dz = (world.dim == 2) ? 0.0f : (s->z - cz);
    float d2 = dx * dx + dy * dy + dz * dz;
    float r = s->hx;
    float nx, ny, nz, pen, dist;
    if (d2 > 1.0e-8f) {
        if (d2 >= r * r) return;
        dist = sqrtf(d2);
        nx = dx / dist;
        ny = dy / dist;
        nz = dz / dist;
        pen = r - dist;
    } else {
        float dl = s->x - minx, dr = maxx - s->x;
        float db = s->y - miny, dt = maxy - s->y;
        float dn = 0.0f, df = 0.0f;
        float best;
        nx = ny = nz = 0.0f;
        best = dl;
        nx = -1.0f;
        if (dr < best) { best = dr; nx = 1.0f; ny = 0.0f; nz = 0.0f; }
        if (db < best) { best = db; nx = 0.0f; ny = -1.0f; nz = 0.0f; }
        if (dt < best) { best = dt; nx = 0.0f; ny = 1.0f; nz = 0.0f; }
        if (world.dim != 2) {
            dn = s->z - minz;
            df = maxz - s->z;
            if (dn < best) { best = dn; nx = 0.0f; ny = 0.0f; nz = -1.0f; }
            if (df < best) { best = df; nx = 0.0f; ny = 0.0f; nz = 1.0f; }
        }
        pen = best + r;
    }
    /* 法線は箱から球へ向ける。 */
    resolve(box, s, nx, ny, nz, pen, bounce);
}

static void collide_plane(body_t *plane, body_t *b, int bounce) {
    float nx = plane->hx, ny = plane->hy, nz = plane->hz;
    float dist, pen, sx, sy, sz;
    if (!dynamic_body(b)) return;
    if (world.dim == 2 && fabsf(nz) > 0.7f) return;
    if (b->shape == PHYS_SHAPE_SPHERE) {
        dist = nx * b->x + ny * b->y + nz * b->z - plane->plane_d;
        pen = b->hx - dist;
        if (pen > 0.0f) resolve(plane, b, nx, ny, nz, pen, bounce);
        return;
    }
    if (b->shape != PHYS_SHAPE_BOX) return;
    sx = b->x - ((nx > 0.0f) ? b->hx : (nx < 0.0f ? -b->hx : 0.0f));
    sy = b->y - ((ny > 0.0f) ? b->hy : (ny < 0.0f ? -b->hy : 0.0f));
    sz = b->z - ((nz > 0.0f) ? b->hz : (nz < 0.0f ? -b->hz : 0.0f));
    dist = nx * sx + ny * sy + nz * sz - plane->plane_d;
    pen = -dist;
    if (pen > 0.0f) resolve(plane, b, nx, ny, nz, pen, bounce);
}

static void collide_pair(body_t *a, body_t *b, int bounce) {
    if (a->shape == PHYS_SHAPE_SPHERE && b->shape == PHYS_SHAPE_SPHERE) {
        collide_spheres(a, b, bounce);
    } else if (a->shape == PHYS_SHAPE_BOX && b->shape == PHYS_SHAPE_BOX) {
        collide_boxes(a, b, bounce);
    } else if (a->shape == PHYS_SHAPE_SPHERE && b->shape == PHYS_SHAPE_BOX) {
        collide_sphere_box(a, b, bounce);
    } else if (a->shape == PHYS_SHAPE_BOX && b->shape == PHYS_SHAPE_SPHERE) {
        collide_sphere_box(b, a, bounce);
    }
}

static int solve_substep(float dt) {
    int i, j, iter, contacts = 0;
    float damp = 1.0f - 0.45f * dt;
    if (damp < 0.0f) damp = 0.0f;
    for (i = 0; i < PHYS_CAP; ++i) {
        body_t *b = &world.bodies[i];
        b->grounded = 0;
        if (!dynamic_body(b)) continue;
        b->vx += world.gx * dt;
        b->vy += world.gy * dt;
        b->vz += world.gz * dt;
        b->vx *= damp;
        b->vy *= damp;
        b->vz *= damp;
        if (world.dim == 2) b->vz = 0.0f;
        clamp_speed(b);
    }
    for (iter = 0; iter < PHYS_ITERS; ++iter) {
        int bounce = (iter == 0);
        int last = (iter == PHYS_ITERS - 1);
        for (i = 0; i < PHYS_CAP; ++i) world.bodies[i].grounded = 0;
        hit_tally = last ? 0 : -1;
        for (i = 0; i < PHYS_CAP; ++i) {
            body_t *p = &world.bodies[i];
            if (!p->active || p->shape != PHYS_SHAPE_PLANE) continue;
            for (j = 0; j < PHYS_CAP; ++j) {
                if (dynamic_body(&world.bodies[j])) collide_plane(p, &world.bodies[j], bounce);
            }
        }
        for (i = 0; i < PHYS_CAP; ++i) {
            body_t *a = &world.bodies[i];
            if (!a->active || a->shape == PHYS_SHAPE_PLANE || a->shape == PHYS_SHAPE_NONE) continue;
            for (j = i + 1; j < PHYS_CAP; ++j) {
                body_t *b = &world.bodies[j];
                if (!b->active || b->shape == PHYS_SHAPE_PLANE || b->shape == PHYS_SHAPE_NONE) continue;
                if (a->inv_mass == 0.0f && b->inv_mass == 0.0f) continue;
                collide_pair(a, b, bounce);
            }
        }
        if (last) contacts = hit_tally;
    }
    for (i = 0; i < PHYS_CAP; ++i) {
        body_t *b = &world.bodies[i];
        float s2;
        if (!dynamic_body(b)) continue;
        if (b->grounded) {
            s2 = b->vx * b->vx + b->vy * b->vy + b->vz * b->vz;
            if (s2 < 36.0f) {
                b->vx = b->vy = b->vz = 0.0f;
            } else if (s2 < 196.0f) {
                b->vx *= 0.55f;
                b->vy *= 0.55f;
                b->vz *= 0.55f;
            }
        }
        b->x += b->vx * dt;
        b->y += b->vy * dt;
        b->z += b->vz * dt;
        if (world.dim == 2) {
            b->z = 0.0f;
            b->vz = 0.0f;
        }
        if (b->x > 4000.0f || b->x < -4000.0f) { b->x = clampf(b->x, -4000.0f, 4000.0f); b->vx = 0.0f; }
        if (b->y > 4000.0f || b->y < -4000.0f) { b->y = clampf(b->y, -4000.0f, 4000.0f); b->vy = 0.0f; }
        if (b->z > 4000.0f || b->z < -4000.0f) { b->z = clampf(b->z, -4000.0f, 4000.0f); b->vz = 0.0f; }
    }
    return contacts;
}

int phys_step(float dt) {
    int n, i, contacts = 0;
    float h;
    if (!(dt > 0.0f)) return 0;
    if (dt > 0.05f) dt = 0.05f;
    n = (int)(dt * 60.0f + 0.999f);
    if (n < 1) n = 1;
    if (n > 4) n = 4;
    h = dt / (float)n;
    for (i = 0; i < n; ++i) contacts = solve_substep(h);
    return contacts;
}

int phys_copy_state(phys_state_t *out, int max_out) {
    int i, n = 0;
    if (!out || max_out < 1) return 0;
    if (max_out > PHYS_CAP) max_out = PHYS_CAP;
    for (i = 0; i < max_out; ++i) {
        const body_t *b = &world.bodies[i];
        phys_state_t *s = &out[i];
        memset(s, 0, sizeof(*s));
        s->id = (uint8_t)i;
        if (!b->active) continue;
        s->active = 1;
        s->shape = b->shape;
        s->flags = b->inv_mass > 0.0f ? 1 : 0;
        if (b->shape == PHYS_SHAPE_PLANE) {
            s->x = b->hx * b->plane_d;
            s->y = b->hy * b->plane_d;
            s->z = b->hz * b->plane_d;
            s->hx = b->hx;
            s->hy = b->hy;
            s->hz = b->hz;
        } else {
            s->x = b->x;
            s->y = b->y;
            s->z = b->z;
            s->vx = b->vx;
            s->vy = b->vy;
            s->vz = b->vz;
            s->hx = b->hx;
            s->hy = b->hy;
            s->hz = b->hz;
        }
        n++;
    }
    return n;
}

int phys_active_count(void) {
    int i, n = 0;
    for (i = 0; i < PHYS_CAP; ++i) if (world.bodies[i].active) n++;
    return n;
}
