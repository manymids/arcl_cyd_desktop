# Adding native applications

English | [日本語](native-app-guide_ja.md)

This guide covers C++ applications compiled into the firmware. Consider the
[MicroPython API](cyd-api_en.md) first if it meets your needs. Native apps require
a firmware rebuild and flash, but can handle demanding per-frame rendering such
as the raycast 3D demo (`raycast_demo.cpp`, Neon Strike 3D).

## 1. How it works

The shell interacts with native apps through the **`NativeApp` structure** in
`include/native_app.h`. It does not branch on specific app IDs. The app's flags,
launcher information and `describe_ui` callback declare its capabilities.

Apps do not own the LCD or a framebuffer. The shell renders in 16-row bands,
passing each band to `render_tile` as a `TileSurface`. Writes outside the band
are ignored by `TileSurface`.

All apps are listed in **one table in `native_registry.cpp`**.

## 2. Implementation steps

### 2.1 Create an implementation file

`firmware/cyd_desktop_shell/my_app.cpp`:

```cpp
#include "native_app.h"

namespace cyd::desktop::native {
namespace {

int ball_y = 40;
int velocity = 3;

void enter() { ball_y = 40; velocity = 3; }
void leave() {}

// Receives elapsed milliseconds; return true when a redraw is needed.
bool update(uint32_t delta_ms) {
    ball_y += velocity;
    if (ball_y < 20 || ball_y > kContentHeight - 20) velocity = -velocity;
    return true;
}

void input(const InputEvent &event) {
    if (event.type == InputType::Tap) velocity = -velocity;
}

// Draw just one band: surface.height rows starting at surface.origin_y.
void render_tile(const TileSurface &surface) {
    surface.fill(rgb565(0, 0, 0));
    for (int y = surface.origin_y; y < surface.origin_y + surface.height; ++y) {
        if (y >= ball_y - 6 && y <= ball_y + 6) surface.hline(154, y, 12, rgb565(255, 200, 0));
    }
}

// Optionally expose an on-screen control through cyd_ui_tree.
void describe_ui(UiNodeFn emit) {
    emit("native.bounce", "button", 0, 0, kScreenWidth, kContentHeight, "Bounce");
}

const NativeApp app{
    "bounce", "BOUNCE", 33,
    enter, leave, update, input, render_tile,
    kAppLaunchable, "BOUNCE", 0x07e0, LauncherIcon::Game, describe_ui,
};

}  // namespace

const NativeApp &bounce_app() { return app; }

}  // namespace cyd::desktop::native
```

### 2.2 Add an entry to the registry

In `firmware/cyd_desktop_shell/native_registry.cpp`:

```cpp
const NativeApp &raycast_demo_app();
const NativeApp &mjp_player_app();
const NativeApp &bounce_app();          // Add this declaration

...
    static const NativeApp *const apps[] = {
        &raycast_demo_app(),
        &mjp_player_app(),
        &bounce_app(),                   // Add this entry
    };
```

### 2.3 Include the file in the build

Add `my_app.cpp` to `target_sources` in
`firmware/cyd_desktop_shell/micropython.cmake`. If the app participates in host
tests, also add it to the `native_app_test.cpp` entry in `tools/run-native-tests.sh`.

### 2.4 Build and flash

Follow "Building and flashing firmware" in the [project README](../README_en.md):
build using ESP-IDF in WSL, then flash using esptool on Windows.
**Do not use `pio run`**; it builds a different firmware.

## 3. `NativeApp` fields

| Field | Meaning |
| --- | --- |
| `id` | Unique app ID, 1–24 characters; passed as `app_id` to `cyd_launch` |
| `title` | Name displayed in the window and `cyd_ui_tree` |
| `target_frame_ms` | Target frame interval |
| `enter` / `leave` | Called when entering/leaving the foreground |
| `update(delta_ms)` | Advance state; return `true` if a redraw is needed |
| `input(event)` | `Down`, `Move`, `Up` or `Tap`, with display coordinates |
| `render_tile(surface)` | Draw a single band |
| `flags` | See below |
| `launcher_label` | Home label, up to eight characters; `nullptr` hides it from Home |
| `launcher_color` | Home icon color in RGB565 |
| `launcher_icon` | `LauncherIcon::Game`, `Movie` or `None` |
| `describe_ui` | Emit controls into `cyd_ui_tree`; use `nullptr` if unnecessary |

### 3.1 Flags

| Flag | Shell behavior |
| --- | --- |
| `kAppLaunchable` | Can be launched from Home and through `cyd_launch` |
| `kAppScriptHosted` | A foreground MicroPython app controls the view; keep its runtime active |
| `kAppVideoSurface` | Target of `cyd.video_begin()`; receives `cyd.video_present()` frames; suppress touch highlighting |

`mjpplayer` has `kAppScriptHosted | kAppVideoSurface`, without `kAppLaunchable`.
The MicroPython player opens the video before entering this view through
`cyd.video_begin()`. Direct launch by ID would otherwise produce an empty player.

## 4. Constraints

- **Home has only one native app slot.** The first launchable registry entry
  with a `launcher_label` occupies it.
- IDs emitted by `describe_ui` must start with `native.` and their bounds must
  fit within `kScreenWidth` × `kContentHeight` (320 × 212). Host tests check this.
- `render_tile` is called once per band on each redraw. Perform expensive
  initialization in `enter`; avoid dynamic allocation in the rendering callback.

## 5. Tests

`firmware/cyd_desktop_shell/tests/native_app_test.cpp` checks every registered
app for unique IDs, required callbacks, label lengths, flag combinations and
the coordinate bounds emitted by `describe_ui`.

```bash
npm run test:native
```

`bridge/firmware-structure.test.mjs` also checks that literal app IDs occur only
in the files defining those apps. Hard-coding an app ID in shell branches fails
that test.
