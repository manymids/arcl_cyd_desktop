#pragma once
#include <stdint.h>
#include <cstddef>

#include "gf2_assets/ship_full.h"
#include "gf2_assets/flame_l.h"
#include "gf2_assets/flame_r.h"
#include "gf2_assets/enemy_a.h"
#include "gf2_assets/enemy_b.h"
#include "gf2_assets/enemy_heavy.h"
#include "gf2_assets/enemy_cruiser.h"
#include "gf2_assets/exp_fireball.h"
#include "gf2_assets/truss.h"
#include "gf2_assets/module.h"
#include "gf2_assets/msl_rocket.h"

namespace cyd::desktop::screen {

struct GameBitmapInfo {
    const uint16_t *data;
    uint16_t width;
    uint16_t height;
};

constexpr uint8_t kBitmapShip = 0;
constexpr uint8_t kBitmapFlameL = 1;
constexpr uint8_t kBitmapFlameR = 2;
constexpr uint8_t kBitmapEnemyA = 3;
constexpr uint8_t kBitmapEnemyB = 4;
constexpr uint8_t kBitmapEnemyHeavy = 5;
constexpr uint8_t kBitmapEnemyCruiser = 6;
constexpr uint8_t kBitmapExp = 7;
constexpr uint8_t kBitmapTruss = 8;
constexpr uint8_t kBitmapModule = 9;
constexpr uint8_t kBitmapRocket = 10;
constexpr uint8_t kGameBitmapCount = 11;

inline const GameBitmapInfo *get_game_bitmap_info(uint8_t id) {
    static const GameBitmapInfo kBitmaps[kGameBitmapCount] = {
        {SHIP_FULL_BMP, SHIP_FULL_W, SHIP_FULL_H},
        {FLAME_L_BMP, FLAME_L_W, FLAME_L_H},
        {FLAME_R_BMP, FLAME_R_W, FLAME_R_H},
        {ENEMY_A_BMP, ENEMY_A_W, ENEMY_A_H},
        {ENEMY_B_BMP, ENEMY_B_W, ENEMY_B_H},
        {ENEMY_HEAVY_BMP, ENEMY_HEAVY_W, ENEMY_HEAVY_H},
        {ENEMY_CRUISER_BMP, ENEMY_CRUISER_W, ENEMY_CRUISER_H},
        {EXP_FIREBALL_BMP, EXP_FIREBALL_W, EXP_FIREBALL_H},
        {TRUSS_BMP, TRUSS_W, TRUSS_H},
        {MODULE_BMP, MODULE_W, MODULE_H},
        {MSL_ROCKET_BMP, MSL_ROCKET_W, MSL_ROCKET_H},
    };
    if (id < kGameBitmapCount) {
        return &kBitmaps[id];
    }
    return nullptr;
}

}  // namespace cyd::desktop::screen
