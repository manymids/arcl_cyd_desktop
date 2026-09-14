"""Pixel Storm: MicroPython game logic with CYD's native sprite batch."""

import time
import cyd


BG_TOP = cyd.rgb(5, 8, 38)
BG_BOTTOM = cyd.rgb(28, 8, 48)
ACCENT = cyd.rgb(35, 225, 255)
PLAYER = cyd.rgb(65, 245, 255)
SHOT = cyd.rgb(255, 245, 105)
ENEMY = cyd.rgb(255, 65, 155)
BULLET_COLORS = (
    cyd.rgb(255, 55, 125), cyd.rgb(255, 105, 45),
    cyd.rgb(255, 220, 55), cyd.rgb(85, 245, 155),
    cyd.rgb(55, 180, 255), cyd.rgb(190, 75, 255),
)

# Fixed-point velocities, 16 units per pixel. All fans move downward while
# alternating curvature turns the lanes into a visible spiral curtain.
FAN = (
    (-25, 15), (-20, 19), (-15, 22), (-10, 24), (-5, 25),
    (0, 26), (5, 25), (10, 24), (15, 22), (20, 19), (25, 15),
)

frame = 0
score = 0
lives = 3
bombs = 2
player_x = 160
player_y = 188
enemy_x = 160
enemy_y = 43
bullets = []       # [x16, y16, vx16, vy16, curve, color]
shots = []         # [x, y]
explosions = []    # [x, y, remaining]
invulnerable = 0

cyd.title("PIXEL STORM")
cyd.game_begin(BG_TOP, BG_BOTTOM, ACCENT)


def spawn_fan():
    direction = -1 if (frame // 40) & 1 else 1
    for index, velocity in enumerate(FAN):
        curve = direction if index & 1 else -direction
        bullets.append([
            enemy_x << 4, (enemy_y + 7) << 4,
            velocity[0], velocity[1], curve,
            (index + frame // 5) % len(BULLET_COLORS),
        ])
    if len(bullets) > 88:
        del bullets[:len(bullets) - 88]


def update_world():
    global frame, score, lives, player_x, player_y, enemy_x, invulnerable
    frame += 1
    phase = frame % 160
    enemy_x = 72 + phase * 2 if phase < 80 else 232 - (phase - 80) * 2

    pressed, touch_x, touch_y = cyd.touch()
    if pressed and 8 <= touch_x <= 311 and 24 <= touch_y < 207:
        player_x = (player_x * 2 + touch_x) // 3
        player_y = (player_y * 2 + touch_y) // 3

    if frame % 5 == 0:
        spawn_fan()
    if frame % 3 == 0:
        shots.append([player_x, player_y - 10])

    for shot in shots:
        shot[1] -= 8
    shots[:] = [shot for shot in shots if shot[1] > 20]

    for bullet in bullets:
        bullet[0] += bullet[2]
        bullet[1] += bullet[3]
        bullet[2] += bullet[4]
        if bullet[2] > 30:
            bullet[4] = -1
        elif bullet[2] < -30:
            bullet[4] = 1
    bullets[:] = [bullet for bullet in bullets
                  if -8 <= (bullet[0] >> 4) <= 328 and 15 <= (bullet[1] >> 4) <= 218]

    hit_shot = None
    for shot in shots:
        if abs(shot[0] - enemy_x) < 14 and abs(shot[1] - enemy_y) < 13:
            hit_shot = shot
            break
    if hit_shot is not None:
        shots.remove(hit_shot)
        score += 100
        explosions.append([enemy_x, enemy_y, 8])

    if invulnerable > 0:
        invulnerable -= 1
    else:
        hit_bullet = None
        for bullet in bullets:
            dx = (bullet[0] >> 4) - player_x
            dy = (bullet[1] >> 4) - player_y
            if dx * dx + dy * dy < 42:
                hit_bullet = bullet
                break
        if hit_bullet is not None:
            bullets.remove(hit_bullet)
            lives -= 1
            invulnerable = 42
            explosions.append([player_x, player_y, 14])
            if lives <= 0:
                lives = 3
                bullets[:] = []
                player_x, player_y = 160, 188

    for burst in explosions:
        burst[2] -= 1
    explosions[:] = [burst for burst in explosions if burst[2] > 0]


def build_frame():
    cyd.game_frame(frame, score, lives, bombs)
    cyd.game_sprite(cyd.GAME_ENEMY, enemy_x, enemy_y, ENEMY, 9)
    for bullet in bullets:
        cyd.game_sprite(cyd.GAME_BULLET, bullet[0] >> 4, bullet[1] >> 4,
                        BULLET_COLORS[bullet[5]], 3)
    for shot in shots:
        cyd.game_sprite(cyd.GAME_SHOT, shot[0], shot[1], SHOT, 5)
    for burst in explosions:
        cyd.game_sprite(cyd.GAME_SPARK, burst[0], burst[1], SHOT,
                        max(2, 16 - burst[2]))
    if invulnerable == 0 or frame & 2:
        cyd.game_sprite(cyd.GAME_PLAYER, player_x, player_y, PLAYER, 8)


build_frame()
while cyd.update():
    update_world()
    build_frame()
    time.sleep_ms(5)
