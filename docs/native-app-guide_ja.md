# ネイティブアプリの追加手順

[English](native-app-guide_en.md) | 日本語

C++ で書いてファームウェアに組み込むアプリの作り方です。MicroPython アプリで足りる場合は
[cyd-api_ja.md](cyd-api_ja.md) を先に検討してください。ネイティブアプリは再ビルドと書き込みが必要な代わりに、
レイキャスト 3D（`raycast_demo.cpp`、Neon Strike 3D）のような毎フレームの重い描画ができます。

---

## 1. 仕組み

シェルはネイティブアプリを **`NativeApp` 構造体**（`include/native_app.h`）だけを通して扱います。
アプリの id を名指しで分岐するコードはシェルにありません。何をしてよいかは、アプリ自身が宣言した
フラグ・ランチャー情報・`describe_ui` から判断します。

アプリは LCD やフレームバッファを持ちません。シェルが画面を 16 行ずつの帯に分けて描画するたびに、
その帯を `TileSurface` として `render_tile` に渡します。`TileSurface` は帯の外への書き込みを無視します。

アプリの一覧は **`native_registry.cpp` の 1 つの表だけ**に置きます。

---

## 2. 手順

### 2.1 実装ファイルを作る

`firmware/cyd_desktop_shell/my_app.cpp`:

```cpp
#include "native_app.h"

namespace cyd::desktop::native {
namespace {

int ball_y = 40;
int velocity = 3;

void enter() { ball_y = 40; velocity = 3; }
void leave() {}

// 経過ミリ秒を受け取り、表示を更新する必要があれば true を返す。
bool update(uint32_t delta_ms) {
    ball_y += velocity;
    if (ball_y < 20 || ball_y > kContentHeight - 20) velocity = -velocity;
    return true;
}

void input(const InputEvent &event) {
    if (event.type == InputType::Tap) velocity = -velocity;
}

// 呼ばれるたびに 1 つの帯（surface.origin_y から surface.height 行）だけを描く。
void render_tile(const TileSurface &surface) {
    surface.fill(rgb565(0, 0, 0));
    for (int y = surface.origin_y; y < surface.origin_y + surface.height; ++y) {
        if (y >= ball_y - 6 && y <= ball_y + 6) surface.hline(154, y, 12, rgb565(255, 200, 0));
    }
}

// 画面上の操作ボタンを cyd_ui_tree に載せる（任意）。
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

### 2.2 登録表に 1 行足す

`firmware/cyd_desktop_shell/native_registry.cpp`:

```cpp
const NativeApp &raycast_demo_app();
const NativeApp &mjp_player_app();
const NativeApp &bounce_app();          // 追加

...
    static const NativeApp *const apps[] = {
        &raycast_demo_app(),
        &mjp_player_app(),
        &bounce_app(),                   // 追加
    };
```

### 2.3 ビルド対象に加える

`firmware/cyd_desktop_shell/micropython.cmake` の `target_sources` に `my_app.cpp` を足します。
ホストテストでも使う場合は `tools/run-native-tests.sh` の `native_app_test.cpp` の行にも足します。

### 2.4 ビルドと書き込み

[プロジェクト README](../README_ja.md) の「ファームウェアのビルドと書込み」の手順どおりです（WSL の ESP-IDF でビルドし、Windows から esptool で書き込む）。
**`pio run` は使わないでください**。別のファームウェアがビルドされます。

---

## 3. `NativeApp` の各項目

| 項目 | 意味 |
|---|---|
| `id` | 1〜24 文字、アプリ間で一意。`cyd_launch` の `app_id` になる |
| `title` | ウィンドウと `cyd_ui_tree` に出る名前 |
| `target_frame_ms` | 目標のフレーム間隔 |
| `enter` / `leave` | 前面に来たとき／離れたとき |
| `update(delta_ms)` | 状態を進める。再描画が要るなら `true` |
| `input(event)` | `Down` / `Move` / `Up` / `Tap` と表示座標 |
| `render_tile(surface)` | 1 つの帯を描く |
| `flags` | 下表 |
| `launcher_label` | Home に出す 8 文字までのラベル。`nullptr` なら Home に出さない |
| `launcher_color` | Home アイコンの色（RGB565） |
| `launcher_icon` | `LauncherIcon::Game` / `Movie` / `None` |
| `describe_ui` | 操作ボタンなどを `cyd_ui_tree` に載せる関数。不要なら `nullptr` |

### 3.1 フラグ

| フラグ | シェルの扱い |
|---|---|
| `kAppLaunchable` | Home のランチャーと `cyd_launch` から起動できる |
| `kAppScriptHosted` | 前面の MicroPython アプリがこのビューを操作する。ビューにいる間もランタイムを止めない |
| `kAppVideoSurface` | `cyd.video_begin()` の切り替え先。`cyd.video_present()` のフレームを受ける。タッチ時の強調表示を出さない |

`mjpplayer` は `kAppScriptHosted | kAppVideoSurface` で、`kAppLaunchable` を持ちません。
MicroPython のプレイヤーが動画を開いてから `cyd.video_begin()` で入るビューなので、id を指定して
直接起動すると空のプレイヤーになってしまうためです。

---

## 4. 制約

- **Home のネイティブ枠は 1 つだけ**です。`launcher_label` を持つ起動可能なアプリのうち、
  登録表で最初のものが表示されます。
- `describe_ui` の各項目は、id を `native.` で始め、`kScreenWidth` × `kContentHeight`
  （320 × 212）の範囲に収めてください。ホストテストがこれを検査しています。
- `render_tile` は描画のたびに帯ごとに呼ばれます。重い初期化は `enter` で済ませ、
  ここでは動的な確保をしないでください。

## 5. テスト

`firmware/cyd_desktop_shell/tests/native_app_test.cpp` が、登録されたすべてのアプリについて、
id の一意性・必須の関数の有無・ラベル長・フラグの組み合わせ・`describe_ui` の座標範囲を検査します。

```bash
npm run test:native
```

加えて `bridge/firmware-structure.test.mjs` が、アプリの id がその定義ファイル以外に文字列で
書かれていないことを検査します。シェルに id を直書きして分岐すると、このテストが失敗します。
