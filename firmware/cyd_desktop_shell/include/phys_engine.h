#ifndef PHYS_ENGINE_H
#define PHYS_ENGINE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 2D / 3D 物理エンジン。IS01 / 12HT 版（jni/phys）から移植した。
 * 積分・衝突検出・衝突応答はすべてこちらで行う。
 * MicroPython はボディの配置と、返ってきた姿勢の描画だけを担当する。
 *
 * 形状は球、軸平行ボックス、無限平面。回転は持たない。
 * dim==2 のとき Z は常に 0 で、衝突は XY だけを見る。
 */

#define PHYS_CAP 48

#define PHYS_SHAPE_NONE   0
#define PHYS_SHAPE_SPHERE 1
#define PHYS_SHAPE_BOX    2
#define PHYS_SHAPE_PLANE  3

/* Python の struct "<9fBBBB" と一致させる。サイズが変わったら両方を直す。 */
typedef struct {
    float x, y, z;
    float vx, vy, vz;
    float hx, hy, hz;
    uint8_t shape;
    uint8_t flags;   /* bit0 = 動的 (質量あり) */
    uint8_t id;
    uint8_t active;
} phys_state_t;

void phys_reset(int dim, float gx, float gy, float gz);

/* id は 0..PHYS_CAP-1。shape 0 でそのスロットを消す。失敗時は -1。
 * 球: (x,y,z) 中心、hx 半径。
 * 箱: (x,y,z) 中心、hx,hy,hz 半辺。
 * 平面: (x,y,z) 平面上の点、hx,hy,hz 法線。法線側が物体のいる側。
 * mass<=0 は静止。 */
int phys_body(int id, int shape,
              float x, float y, float z,
              float hx, float hy, float hz,
              float mass, float restitution, float friction);

int phys_velocity(int id, float vx, float vy, float vz);

/* dt は秒。返り値は最後のサブステップで残っていた接触数。 */
int phys_step(float dt);

/* out[0..PHYS_CAP) をスロット順に書く。返り値は有効ボディ数。 */
int phys_copy_state(phys_state_t *out, int max_out);

int phys_active_count(void);

#ifdef __cplusplus
}
#endif

#endif
