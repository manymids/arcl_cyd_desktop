# `cyd` API リファレンス

[English](cyd-api_en.md) | 日本語

ARCL CYD Desktop 上で動く MicroPython アプリ向けの API をまとめたものです。フェーズ文書
（p9 / p14 / p19 / p20 など）に散っていた説明を一本にしました。

- 実装: `firmware/cyd_desktop_board/modules/cyd.py`
- この文書に載っていない公開関数があると、`bridge/sdk-docs.test.mjs` が失敗します。
  下の上限値もファームウェアの定数と照合しています。

---

## 1. アプリの構成

```
/sd/apps/<app-id>/
    manifest.json   任意。アプリの名前と入口
    main.py         入口（manifest.json の entry で変更可）
    *.jpg など      アセット。サブディレクトリは使えない
    data/           cyd.storage_* と cyd.checkpoint() の保存先（自動作成）
```

`<app-id>` はディレクトリ名そのもので、1〜24 文字の `A-Z a-z 0-9 _ . -` です。

### 1.1 manifest.json

```json
{
  "id": "console",
  "title": "CONSOLE",
  "entry": "main.py",
  "api": 4,
  "version": "0.0.1"
}
```

| キー | 必須 | 規則 |
|---|---|---|
| `title` | ○ | 1〜12 文字の `A-Z a-z 0-9 空白 _ . -`。Scripts と Home に表示される |
| `id` | | 書く場合はディレクトリ名と一致すること |
| `entry` | | 実行する `.py` ファイル名。既定は `main.py` |
| `api` | | アプリが必要とする API レベル（正の整数）。ファームウェアの `cyd.API_LEVEL` より大きいと起動しない。既定は 1 |
| `version` | | 1〜16 文字の文字列。表示用で、動作には影響しない |

- 上記以外のキーは誤記（`tittle` など）とみなして拒否します。
- manifest.json が無いアプリも有効です。タイトルはアプリ id、入口は `main.py` になります。
- manifest が不正なアプリは Scripts の一覧に出ません。理由はイベントログ（`cyd_logs`）に
  `skip <app-id>: <field>: <理由>` として残ります。
- 規則の実装は `_manifest.py`（デバイス側）と `bridge/manifest.mjs`（PC 側）の 2 つで、
  どちらも `firmware/cyd_desktop_board/tests/manifest-cases.json` の同じケースで検証しています。

### 1.2 インストール

アプリのディレクトリをまるごと転送します。manifest.json はデバイスへ送る前に PC 側で検査されます。

```bash
node bridge/deploy.mjs examples/console --pin --launch
```

| オプション | 意味 |
|---|---|
| `--pin` | Home にショートカットを置く。同じアプリのショートカットが既にあれば、それを更新する |
| `--launch` | 転送後に起動する |
| `--no-replace` | デバイス上に同名のファイルがあれば失敗させる（既定では置き換える） |
| `--port COM6` | ポートを指定する。省略時は `CYD_DESKTOP_PORT`、それも無ければ CH340 を自動検出 |

- ローカルのディレクトリ名がアプリ id と違う場合は、manifest.json の `id` に書きます
  （例: `examples/pixel_barrage` は `pixelstorm` としてインストールされる）。
- `README*`、`__pycache__/`、`*.pyc`、ドットファイルは転送しません。
- 大きなアセットを 1 つだけ追加する場合:

  ```bash
  node bridge/deploy.mjs --app mjpplayer --asset movie.mjp --fast
  ```

  `--fast` は転送中だけ 921,600 bps に切り替えます。

MCP クライアントからは `cyd_app_deploy`（ディレクトリ単位）と `cyd_package_upload`
（1 ファイル。`encoding: "base64"` でバイナリも可）を使います。`cyd_app_deploy` が読めるのは
`CYD_DESKTOP_APPS_ROOT`（既定はリポジトリの `examples/`）の中だけです。

---

## 2. 実行モデル

- 前面で動くアプリは常に 1 つです。Home、Start、長押しで終了を要求されます。
- 描画要求はまとめられ、**`cyd.update()` を呼んだときに 1 回で画面へ反映**されます。
- `cyd.update()` は Task Watchdog への生存通知も兼ねます。**15 秒以上呼ばないとボードがリセット**され、
  Home に戻ります。`cyd.wifi_connect()` と `cyd.http_get()` は待機中に自分で通知するので、
  その間は呼ばなくて構いません。
- 1 回の起動の実行時間は 5 分までです（`cyd_pause` などで止めていた時間は数えません）。
  開いたままにするアプリ（コンソールなど）は `cyd.no_time_limit()` で外せます。
- ESP-IDF の空きヒープが 8,192 バイト未満になり、かつそのアプリが起動時から 4,096 バイト以上減らした
  場合は停止されます。
- MicroPython のヒープは足りなくなると ESP-IDF のヒープへ広がり、その分は再起動まで戻りません。
  無線モードが Bluetooth のときは、キーボードの接続用に 24 KB を残してそれ以上は広がらず、
  足りないアプリは `MemoryError` で止まります（Home 表示中の空きは OFF／Wi-Fi で約 146 KB、Bluetooth で約 54 KB）。
- 停止された理由はイベントログに残ります（`Stopped: heap ...`、`Stopped: 5 minute runtime limit`）。

### 2.1 エラー

- 例外で終了すると、イベントログには `ERROR: <型>: <メッセージ>` が 48 文字で残ります。
- **トレースバックの全文**は MCP の `cyd_app_error`（プロトコルのコマンドは `desktop_app_error`）で読めます。
  返るのは**直近に失敗した 1 件**で、どのアプリのものかは結果の `app_id` で判断します。
  アプリごとの記録は SD の `/sd/desktop/errors/<app-id>.txt` に残ります（1,023 文字まで）。
- `sys.exit()` は正常終了として扱います。
- 例外で終了すると、トレースバックを画面に表示したままにします。Home で閉じます
  （動画を再生していたアプリは表示できないので Home に戻ります）。
- ハードウェアキーボードの **Ctrl+C** は、実行中のアプリに `KeyboardInterrupt` を送ります
  （`while True: pass` のようなループも止まります）。捕まえなければアプリは `Stopped: Ctrl+C` で終了します。

### 2.2 print()

画面に何も描いていないアプリの `print()` は、画面いっぱいの出力欄（22 行）に表示されます。
Scripts から実行したスクリプトの結果をそのまま読めます。アプリが `cyd.title()` などで描き始めた後の
`print()` は、ステータス行（`cyd.log()` と同じ場所）に最後の 1 行だけ出ます。

### 2.3 最小のアプリ

```python
import cyd

count = 0

def draw():
    cyd.clear()
    cyd.title("COUNTER")
    cyd.text(16, 60, "COUNT %d" % count)
    cyd.button("add", 16, 120, 120, 40, "ADD", add)

def add():
    global count
    count += 1
    draw()          # 変化したときだけ描き直す

draw()
cyd.run()           # Home / Start / 長押しで抜ける
```

- `cyd.clear()` はテキスト・ボタン・図形とボタンのコールバックをすべて消すので、描き直すときは
  必要なものをすべて呼び直します。
- 変化が無いのに毎フレーム `cyd.clear()` から描き直すと、そのたびに画面全体を転送します。
  状態が変わったときだけ描き直してください。

---

## 3. 画面とウィンドウ

画面は 320×240。アプリが描けるのは上端のタイトル帯とタスクバー（y = 212 以降）を除いた範囲です。
色は RGB565 の整数で、`cyd.rgb()` で作れます。

### 3.1 定数

| 名前 | 値 |
|---|---|
| `cyd.API_LEVEL` | ファームウェアが提供する API レベル（現在 9） |
| `cyd.BLACK` `cyd.NAVY` `cyd.BLUE` `cyd.GREEN` `cyd.CYAN` `cyd.RED` `cyd.MAGENTA` `cyd.ORANGE` `cyd.YELLOW` `cyd.WHITE` | RGB565 の色 |
| `cyd.GAME_PLAYER` `cyd.GAME_ENEMY` `cyd.GAME_BULLET` `cyd.GAME_SHOT` `cyd.GAME_SPARK` | スプライトの種類（0〜4） |
| `cyd.TILE_FLIP_X` `cyd.TILE_FLIP_Y` `cyd.TILE_BEHIND` | `cyd.tile_sprite()` のフラグ（1、2、4） |
| `cyd.KEY_DOWN` `cyd.KEY_PRESSED` `cyd.KEY_RELEASED` | `cyd.key_state()` が返すビット（1、2、4） |
| `cyd.PHYS_SPHERE` `cyd.PHYS_BOX` `cyd.PHYS_PLANE` | `cyd.phys_body()` の形（1、2、3） |
| `cyd.PHYS_CAP` `cyd.PHYS_STRIDE` | 物体の数の上限（48）と `cyd.phys_state()` の 1 件のバイト数（40） |
| `cyd.ALIGN_LEFT` `cyd.ALIGN_CENTER` `cyd.ALIGN_RIGHT` | `cyd.overlay_text()` の揃え（0、1、2） |
| `cyd.IMAGE_CAP` `cyd.OVERLAY_CAP` | 画像の数（16）と 1 フレームのオーバーレイの数（24）の上限 |
| `cyd.HTTP_MAX_BYTES` | `cyd.http_get()` が受け取る本文の上限（32,768） |

### 3.2 上限

超えた呼び出しは例外にならず、**`False` を返して無視されます**。戻り値を確認してください。

| 対象 | 上限 | 定数（ファームウェア） |
|---|---:|---|
| テキスト | 10 件 | `kAppTextCapacity` |
| ボタン | 4 件 | `kAppButtonCapacity` |
| 図形 | 48 件 | `kAppPrimitiveCapacity` |
| ゲームスプライト | 128 件 | `kGameSpriteCapacity` |
| ゲーム三角形 | 256 件 | `kGameTriangleCapacity` |
| ゲーム画像 | 16 件 | `kGameImageCapacity` |
| オーバーレイ | 24 件 | `kGameOverlayCapacity` |
| コンソール行 | 22 行 | `kConsoleMaxLines` |

### 3.3 基本の UI

#### `cyd.title(value)`
ウィンドウのタイトルを設定します。24 文字まで。

#### `cyd.clear()`
テキスト・ボタン・図形と、登録済みのボタンコールバックをすべて消します。

#### `cyd.text(x, y, value)` → `bool`
テキストを 1 行置きます。48 文字まで。x は 16〜300、y は 47〜199 に丸められます。

#### `cyd.button(identifier, x, y, width, height, label, on_tap=None)` → `bool`
ボタンを置き、タップされたら次の `cyd.update()` の中で `on_tap()` を呼びます。
`identifier` は 16 文字まで、`label` は 24 文字まで。x は 16〜290、y は 47〜190、
幅は 24〜140、高さは 16〜48 に丸められます。

#### `cyd.log(value)`
イベントログに 1 行残します。48 文字を超える分は切れます。

### 3.4 図形

いずれも成功すると `True`。座標は x −320〜640、y −240〜480 に丸められ、画面外は描かれません。

#### `cyd.rgb(red, green, blue)` → `int`
0〜255 の各成分を RGB565 の値にします。

#### `cyd.pixel(x, y, color=WHITE)` → `bool`
#### `cyd.line(x0, y0, x1, y1, color=WHITE, width=1)` → `bool`
#### `cyd.rect(x, y, width, height, color=WHITE, stroke=1)` → `bool`
#### `cyd.fill_rect(x, y, width, height, color=WHITE)` → `bool`
#### `cyd.circle(x, y, radius, color=WHITE, stroke=1)` → `bool`
#### `cyd.fill_circle(x, y, radius, color=WHITE)` → `bool`

#### `cyd.triangle(x0, y0, x1, y1, x2, y2, color=WHITE, fill=True)`
#### `cyd.fill_triangle(x0, y0, x1, y1, x2, y2, color=WHITE)`
三角形を描きます。`fill=False` で輪郭だけ。ほかの図形と違い、件数の上限が無く、戻り値もありません。

---

## 4. 更新ループ

#### `cyd.update()` → `bool`
まとめた描画を画面へ反映し、Watchdog に生存を通知し、ボタンのタップを処理します。
**アプリが終了を要求されていれば `False`** を返すので、ループを抜けてください。

#### `cyd.should_stop()` → `bool`
終了を要求されているかどうかだけを返します（描画も通知もしません）。

#### `cyd.run(step=None, interval_ms=30)`
`cyd.update()` が `False` を返すまで、`step()` と待機を繰り返します。待機は最短 10 ms。

---

## 5. ゲーム描画

フレームバッファを持たず、ネイティブ側がスプライトを描きます。

#### `cyd.game_begin(background_top=NAVY, background_bottom=BLACK, accent=CYAN)`
ゲーム描画モードに切り替えます。背景は上下 2 色のグラデーション。

#### `cyd.game_frame(frame, score=0, lives=3, bombs=0)`
新しいフレームを始め、前のフレームのスプライトを消します。`lives` は 0〜5、`bombs` は 0〜3。

#### `cyd.game_sprite(kind, x, y, color=WHITE, size=3)` → `bool`
スプライトを 1 つ置きます。`kind` は `cyd.GAME_*`、`size` は 1〜16。1 フレーム 128 個まで。

#### `cyd.game_triangle(x0, y0, x1, y1, x2, y2, color=WHITE, wire=False)` → `bool`
三角形を 1 つ置きます。スプライトより奥に描かれます。`wire=True` で輪郭だけを描きます。1 フレーム 256 個まで。

#### `cyd.game_quad(x0, y0, x1, y1, x2, y2, x3, y3, color=WHITE, wire=False)` → `bool`
四角形を三角形 2 つとして置きます。2 つとも置けたときだけ `True`。

#### `cyd.game_sprites(records)` → `int`
スプライトをまとめて置きます（API レベル 7 以降）。`records` は `struct.pack("<BhhHB", kind, x, y, color, size)` の
8 バイトを並べたバッファです。置けた数を返します。長さが 8 の倍数でなければ `ValueError`。

#### `cyd.game_draw_stats()` → `(sprites, triangles, dropped_sprites, dropped_triangles)`
このフレームに置いたスプライトと三角形の数、上限で断られた数を返します（API レベル 7 以降）。
どれも `cyd.game_frame()` で 0 に戻ります。

#### `cyd.touch()` → `(pressed, x, y)`
いまの物理タッチ状態を返します。ゲームループでの低遅延なポーリング用です。

#### `cyd.touch_pressed()` → `(x, y)` または `None`
押されている間はその座標、離れていれば `None`（API レベル 7 以降）。

#### `cyd.touch_edge()` → `(x, y)` または `None`
押し始めたときに 1 回だけ座標を返し、それ以外は `None`（API レベル 7 以降）。

#### `cyd.touch_points()` → `tuple`
押されている点を `((x, y, id), ...)` で返します（API レベル 7 以降）。CYD の抵抗膜パネルは 1 点だけなので、
空か 1 個です。

### 5.1 タイルマップ（API レベル 5 以降）

横スクロールのゲーム向けに、タイルマップ・スプライト・文字を 320×240 の全画面にネイティブで描きます
（タスクバーも覆います）。フレームは `cyd.tile_present()` を呼んだ MicroPython のタスクで描かれ、
16 行ずつの帯で転送されます。例は `examples/pip_quest` です。

絵はすべて 16×16 のセルで、1 ピクセル 4 ビット（上位ニブルが左）、1 セル 128 バイトです。
渡したバッファはコピーせずそのまま使うので、`tilemap` のバイトを書き換えると次のフレームからマップが変わります。
バッファの大きさは変えないでください。

#### `cyd.tile_begin(palette, tiles, sprites, tilemap, width, height, origin_y=0)` → `True`
タイルマップのビューに切り替えます。
`palette` はビッグエンディアン RGB565 の 16 色を 1〜4 組（32 バイト単位）。タイルはパレット 0 を使い、
タイル 0 は空（パレット 0 の色 0 で塗る）です。`sprites` のセルでは色 0 が透明です。
`tilemap` は `width`×`height`（高さ 255 まで。API レベル 5 では 32 まで）のタイル番号を行ごとに並べたもので、画面の `origin_y` 行目から描きます。
不正な引数は `ValueError` です。別の場面へ移るときは、もう一度呼んでバッファを取り替えます。

#### `cyd.tile_palette(palette)`
パレットを取り替えます。組の数は `tile_begin()` と同じでなければなりません。

#### `cyd.tile_frame(scroll_x, scroll_y=0)`
次のフレームを始めます。マップを `scroll_x`・`scroll_y` ピクセルずらし、スプライトを消します。
`scroll_y` は API レベル 6 以降です。

#### `cyd.tile_sprite(cell, x, y, flags=0)` → `bool`
スプライトのセルを画面座標に置きます。1 フレーム 64 個まで。`flags` は `cyd.TILE_FLIP_X`・`cyd.TILE_FLIP_Y`・
`cyd.TILE_BEHIND`（マップが色 0 でない所には描かない）の和に、パレット番号×16 を足したものです。

#### `cyd.tile_rect(slot, x, y, width, height, fill=0, border=-1)` → `bool`
塗りつぶした長方形を 8 個のスロット（0〜7）の 1 つに置きます（API レベル 6 以降）。スプライトの上、文字の下に描かれ、
ウィンドウや画面上のボタンに使えます。`fill`・`border` はパレット 0 の色番号で、`border` が -1 なら枠なし、
それ以外は角を落とした 2 ピクセルの枠です。幅か高さが 0 なら消えます。フレームをまたいで残ります。

#### `cyd.tile_text(slot, x, y, value, color=1, scale=1)` → `bool`
文字列を 16 個のスロット（0〜15。API レベル 5 では 0〜7）の 1 つに置きます。32 文字まで、色はパレット 0 の番号、倍率は 1〜4。
フレームをまたいで残り、`""` で消えます。

#### `cyd.tile_present()` → `bool`
組み立てたフレームを表示します。タイルマップのビューが前面になくなると `False`（アプリは終了してください）。

---

## 6. 画像と動画

### 6.1 結果コード

失敗すると `OSError(<コード>)` を送出します（`cyd.video_present()` の -4 だけは `False`）。

| コード | 意味 |
|---:|---|
| -1 | 引数が不正（空のパス、4 バイト未満のデータなど） |
| -2 | 表示が使用中で 1 秒以内に取れなかった |
| -3 | LCD への出力に失敗した |
| -4 | 動画ビューではない（`cyd.video_present()` のみ。停止を意味する） |
| -5 | ファイルを開けなかった |
| -6 | 1/8 に縮小しても画面に収まらない大きさ |
| -7 | デコード用のバッファ（約 14 KB）を確保できない。メモリが空いてから再試行する |
| -11 以下 | JPEG デコーダのエラー（TJpgDec の JRESULT を −(値)−10 で表したもの） |

#### `cyd.jpeg(source, x=-1, y=-1)` → `True`
ベースライン JPEG を表示します。`source` はファイルパス（`str`）か、バイト列です。
`x` と `y` が −1 なら中央に置きます。画面より大きい画像は 1/2・1/4・1/8 のいずれかに自動で縮小します。

#### `cyd.video_begin()` → `bool`
ネイティブの動画ビューに切り替えます。以後、アプリは Python から SD を読みながら
フレームを渡し続けます。

#### `cyd.video_present(jpeg)` → `bool`
240×240 のベースライン JPEG を 1 フレーム表示します。動画ビューを離れていれば `False`。
フレームごとに新しいバイト列を作ると、そのたびに最大 65 kB の確保と GC が起きます。
最大フレーム長のバッファを 1 つ確保して使い回し、`readinto()` と `memoryview` で読み込んでください
（`examples/mjp_player/main.py`）。なお GC ヒープは最初の大きな確保で一度だけ拡張され、
その分は再起動まで戻りません。動画の再生には約 49 KB が必要なので、Bluetooth モードでは再生できません。

---

## 7. コンソール

仮想キーボード付きのテキスト端末の見た目を、ネイティブ側で描きます。キー入力の解釈はアプリが行います。

#### `cyd.console_begin()`
コンソール描画モードに切り替えます。仮想キーボードと入力行を表示し、プロンプトは `>>> ` です。

#### `cyd.console_options(keyboard=True, input=True)`
仮想キーボードと入力行の表示を切り替えます。見える行数は、キーボードありで 10 行、入力行だけで 21 行、
どちらもなしで 22 行です（API レベル 4 以降）。

#### `cyd.console_line(index, text, color=WHITE)`
端末領域の `index` 行目（0〜21）を設定します。51 文字まで。

#### `cyd.console_input(text, cursor_pos, prompt=None)`
入力行の文字列（51 文字まで）とカーソル位置（0〜50）を設定します。`prompt` を渡すとプロンプト（7 文字まで）も
変わります（API レベル 4 以降）。

#### `cyd.console_keyboard(mode=0, pressed_key=-1)`
キーボードの種類（0: 小文字、1: 大文字、2: 記号）と、押下中として強調するキー番号を設定します。

キーの位置と文字は、ネイティブのエディタと共通の配置から次の関数で引けます。アプリ側にキーの表を持つ必要はありません
（API レベル 3 以降）。

#### `cyd.console_key_at(x, y)`
画面座標 `(x, y)` にあるキーの番号（0〜50）を返します。キーボードの外なら -1 です。`cyd.touch()` の座標をそのまま渡せます。

#### `cyd.console_key_char(index, mode=0)`
キー `index` が `mode`（0: 小文字、1: 大文字、2: 記号）で入力する 1 文字を返します。範囲外の `index` は `ValueError` です。
文字を入力しないキーは次の定数を返します。

| 定数 | キー |
|---|---|
| `cyd.KEY_BACKSPACE` | BS |
| `cyd.KEY_CAPS` | CAP（大文字との切り替え） |
| `cyd.KEY_SYMBOLS` | SYM（記号との切り替え） |
| `cyd.KEY_ESCAPE` | ESC |
| `cyd.KEY_ENTER` | ENTER（`"\n"`） |

### 7.1 ハードウェアキーボード

Bluetooth キーボード（Settings > Wireless > KEYBOARD でペアリング）のキーは、前面にあるものへ届きます。
エディタではそのまま編集に使え、実行中のアプリには `cyd.key()` で届きます（API レベル 4 以降）。
押し続けたキーは 0.5 秒後から繰り返します。キー配列は Settings の KEYS JIS / KEYS US で選びます。
キーボードの接続中、エディタは仮想キーボードを隠して 16 行を表示します（切断すると戻ります）。
コンソール型のアプリは `cyd.keyboard_connected()` を見て `cyd.console_options(keyboard=False)` に切り替えます
（`examples/console` がその例です）。
タスクバー右端のキーボード記号は、接続中が緑、ペアリング済みで未接続が黄色です。接続・切断の瞬間には 3 秒間の通知が出ます。
スリープしたキーボードはキーを押すと再接続しますが、つながるまでに押したキーは届きません。

#### `cyd.key()` → `str` または `None`
次のキーを返します。無ければ `None`。文字のキーはその文字（`"a"`、`"A"`、`"?"`）、
それ以外は名前です: `"ENTER"`、`"BACKSPACE"`、`"DELETE"`、`"TAB"`、`"ESC"`、`"UP"`、`"DOWN"`、`"LEFT"`、
`"RIGHT"`、`"HOME"`、`"END"`、`"PAGEUP"`、`"PAGEDOWN"`、`"INSERT"`、`"F1"`〜`"F12"`。
修飾キーは前に付きます: `"CTRL+S"`、`"ALT+X"`、`"SHIFT+TAB"`。Ctrl+C は返らず、`KeyboardInterrupt` になります。

#### `cyd.keys_down()` → `tuple`
いま押されているキーの名前を返します（API レベル 5 以降）。ゲームで押しっぱなしを見るためのものです。
文字のキーは修飾なしの文字（`"z"`、`"1"`、空白は `" "`）、それ以外は `cyd.key()` と同じ名前（`"LEFT"`、`"ENTER"`）で、
最後に押されている修飾キー `"SHIFT"`・`"CTRL"`・`"ALT"` が付きます。多くのキーボードは同時に 6 キーまで報告します。
`desktop_key` で送ったキーは 250 ms の間押されているものとして報告します。

#### `cyd.key_state(name, clear=True)` → `int`
1 つのキーの状態を `cyd.KEY_DOWN`（押されている）・`cyd.KEY_PRESSED`（押された）・`cyd.KEY_RELEASED`（離された）の
ビットで返します（API レベル 7 以降）。`name` は `cyd.keys_down()` の名前です（`"w"`、`" "`、`"LEFT"`、`"SHIFT"`）。
押された・離されたは読むまで残り、`clear=False` なら読んでも残ります。最初に呼んだときから、呼ぶたびと
`cyd.update()` のたびに押し離しを記録します。

#### `cyd.keyboard_connected()` → `bool`
Bluetooth キーボードが接続中なら `True`。

#### `cyd.no_time_limit()`
このアプリの 5 分の実行時間制限を外します。15 秒の Watchdog は外れません。

エディタのキー操作: 矢印、Home / End（Ctrl 付きで先頭 / 末尾）、PageUp / PageDown、Tab（4 桁ごと）、Shift+Tab（字下げを戻す）、
Enter（字下げを引き継ぎ、`:` の後は 1 段深く）、Delete、Ctrl+S（保存）、Ctrl+O（開く）。

プロトコルの `desktop_key {"key": "CTRL+S"}`（MCP の `cyd_key_press`・`cyd_type_text`）でも、同じ名前でキーを送れます。

---

## 8. 保存

保存先は `/sd/apps/<app-id>/data/` です。

#### `cyd.storage_put(key, value)`
文字列を保存します。`key` は 1〜24 文字の `A-Z a-z 0-9 _ -`、`value` は 512 バイトまで。

#### `cyd.storage_get(key, default=None)` → `str`
保存した文字列を返します。無ければ `default`。

#### `cyd.checkpoint(value)` → `True`
JSON にできる値を、次回の起動へ引き継ぐために保存します（4,096 バイトまで）。
書き込みの途中で電源が切れても、直前のチェックポイントが残るように保存します。

#### `cyd.restore(default=None)`
チェックポイントを読み込みます。無ければ `default`。

#### `cyd.clear_checkpoint()` → `bool`
チェックポイントを削除します。削除したら `True`。

---

## 9. ネットワーク

Wi-Fi と Bluetooth はヒープに同時に載らないため、どちらを使うかは Settings > Wireless の
無線モード（OFF / WI-FI / BLUETOOTH、初期値 OFF）で起動時に決まります。変更は再起動後に有効です。
無線モードが Wi-Fi でないとき、`cyd.wifi_init()`・`cyd.wifi_connect()`・`cyd.http_get()` は
理由を書いた `RuntimeError` を出し、`cyd.wifi_isconnected()` は `False`、`cyd.wifi_ip()` は `None` を返します。
`cyd` を通さずに `network.WLAN()` や `espnow` を使った場合も、ファームウェアが Wi-Fi の起動を止め、
`OSError: Wi-Fi is off: ...` になります。

#### `cyd.radio_mode()` → `str`
この起動の無線モード。`"off"`・`"wifi"`・`"bluetooth"` のいずれか（API レベル 4 以降）。

#### `cyd.wifi_enabled()` → `bool`
無線モードが Wi-Fi なら `True`（API レベル 4 以降）。

Wi-Fi の資格情報は `/sd/wifi.json`（`{"ssid": "...", "password": "..."}`）から読めます。

#### `cyd.wifi_init()` → `WLAN`
ステーション用の `network.WLAN` を返します（初回に作成）。

#### `cyd.wifi_connect(ssid=None, password=None, timeout_ms=15000)` → `str`
接続して IPv4 アドレスを返します。`ssid` を省略すると `/sd/wifi.json` を使います。
タイムアウトすると `RuntimeError`。待機中も Watchdog に通知します。

#### `cyd.wifi_isconnected()` → `bool`
接続中で IP アドレスがあれば `True`。

#### `cyd.wifi_ip()` → `str` または `None`
現在の IPv4 アドレス。

#### `cyd.wifi_disconnect()`
切断してインターフェースを止めます。

> Wi-Fi ドライバは一度起動すると、アプリが終了しても約 49 kB のメモリを保持します（再起動で解放）。

#### `cyd.http_get(url, timeout=10)` → `str`
`http://` の GET を行い、本文を文字列で返します。`https://` は使えません。
`timeout` は全体の秒数です。本文が `cyd.HTTP_MAX_BYTES` を超える場合と、ステータスが 2xx 以外の場合は
`OSError`。待機中も Watchdog に通知します。

## 10. 共通ヘルパー

Android 版（IS01 / 12HT）の `cyd` と同じ名前・引数で使える補助です（API レベル 7 以降）。
2 つの API レベルは別々に番号が進むので、アプリを移すときは `manifest.json` の `api` を CYD の番号にしてください。

#### `cyd.screen_size()` → `(320, 240)`
画面の大きさを返します。

#### `cyd.hypot(x, y)` → `float`
`sqrt(x*x + y*y)` を返します。

#### `cyd.features()` → `tuple`
このファームウェアが持つ機能の名前を並べ替えて返します。いまは `"image"`、`"keyboard"`、`"overlay"`、`"phys"`、`"tile"`、`"touch_points"`、`"video"`、
無線モードが Wi-Fi のときは `"wifi"`。アプリは `"phys" in cyd.features()` のように確かめてから使えます。

#### `cyd.last_error()` → `str` または `None`
直前の読み込み・初期化が失敗した理由。成功すると `None` に戻ります。アプリが始まるたびに `None` になります。

## 11. 物理演算（API レベル 8 以降）

球・軸に平行な箱・無限平面を、回転なしで動かします。積分と衝突はネイティブ側が行い、アプリは物体を置いて、
`cyd.phys_state()` が返す位置を描きます。Android 版（IS01 / 12HT）と同じ API です。
世界はアプリが始まるたびに空になります（重力は `(0, -520, 0)`）。

```python
import struct
cyd.phys_reset(2, (0, 400, 0))                  # 2D、画面下向きの重力
cyd.phys_body(0, cyd.PHYS_PLANE, (0, 200, 0), (0, -1, 0), mass=0)   # y=200 の床
cyd.phys_body(1, cyd.PHYS_SPHERE, (160, 40, 0), 8, restitution=0.6)
state = bytearray(cyd.PHYS_CAP * cyd.PHYS_STRIDE)
while cyd.update():
    cyd.phys_step(1 / 30)
    cyd.phys_state(state)
    x, y = struct.unpack_from("<2f", state, 1 * cyd.PHYS_STRIDE)
```

#### `cyd.phys_reset(dim=3, gravity=(0.0, -520.0, 0.0))`
世界を空にします。`dim` は 2 か 3。2 のとき z は常に 0 です。`gravity` の単位は 1 秒あたりの速度です。

#### `cyd.phys_body(body_id, shape, pos, size, mass=1.0, restitution=0.3, friction=0.4)` → `int`
スロット `body_id`（0〜47）に物体を置き直し、その番号を返します。球は `size` が半径、箱は `(hx, hy, hz)` の半分の大きさ、
平面は `pos` が平面上の点で `size` が法線です（物体は法線の向く側にいます）。`mass` が 0 以下なら動きません。
`shape` を 0 にするとそのスロットを消します。番号や形が正しくないと `ValueError`。
跳ね返りは 2 つの物体の `restitution` の小さい方、摩擦は `friction` の小さい方を使います。

#### `cyd.phys_velocity(body_id, vel)` → `bool`
動く物体の速度 `(vx, vy, vz)` を設定します。動かない物体や空のスロットなら `False`。

#### `cyd.phys_step(dt)` → `int`
`dt` 秒進めます。接触している組の数を返します。物体 48 個で 1 回約 10 ms かかります（実機で測定）。

#### `cyd.phys_state(buf)` → `int`
`buf`（`cyd.PHYS_CAP * cyd.PHYS_STRIDE` バイト以上の `bytearray`）に、スロット順で 48 件を書きます。
1 件は `struct` の `"<9fBBBB"`: x, y, z, vx, vy, vz, hx, hy, hz, 形, フラグ（bit 0 が動く物体）, 番号, 使用中。
使用中の物体の数を返します。平面の x, y, z は原点に最も近い平面上の点です。

## 12. 画像とオーバーレイ（API レベル 9 以降）

ゲーム描画（`cyd.game_begin()`）に、アプリの画像と、画面の上に重ねる文字・長方形を足します。
Android 版（IS01 / 12HT）と同じ呼び方です。物理演算・画像・オーバーレイ・`cyd.touch_edge()`・`cyd.key_state()` を
まとめて使う例が `examples/bounce_lab` です（`npm run install:bounce`）。

```python
cyd.game_begin()
ship = cyd.load_image("ship.rgb", 32, 24)       # アプリのフォルダにある RGB565
while cyd.update():
    cyd.game_frame(frame)
    cyd.game_bitmap(ship, 160, 120, scale=150)
    cyd.overlay_rect(0, 190, 320, 20, cyd.BLACK)
    cyd.overlay_text(160, 196, "READY", cyd.YELLOW, 2, align=cyd.ALIGN_CENTER)
```

#### `cyd.load_image(name, width, height)` → `int`
アプリのフォルダ（`/sd/apps/<id>/`）にある、ヘッダなしのリトルエンディアン RGB565 の画像を読み、
`cyd.game_bitmap()` に渡す番号（0〜15）を返します。色 0 は透明です。`name` はファイル名だけ
（英数字と `_.-`、24 文字まで）。大きさは 320×240 まで。画素はアプリのヒープに置かれ、アプリの終了で解放されます
（64×64 で 8 KB）。大きさが違う、ファイルが無い・長さが合わないと `ValueError` / `OSError`、
ヒープか 16 個の枠が足りないと `MemoryError` になり、`cyd.last_error()` にも理由が入ります。

#### `cyd.game_bitmap(id, x, y, scale=100, flags=0)` → `bool`
読み込んだ画像を `(x, y)` を中心に、`scale` パーセントの大きさで置きます。`flags` の bit 0 で左右を反転します。
`cyd.game_sprite()` と同じ 128 個の枠を使います。番号が読み込まれていないか、枠が一杯なら `False`。
32×24 の画像を拡大・縮小して 20 個とオーバーレイ 24 個を毎フレーム描くと、約 14 fps です（実機で測定）。

#### `cyd.overlay_text(x, y, value, color=WHITE, scale=1, shadow=0x0821, align=ALIGN_LEFT)` → `bool`
5×7 の文字（40 文字まで）をゲーム画面の上に重ねます。`scale` は 1〜6、`shadow` は右下へ 1 ドット（×`scale`）ずらして
先に描く影の色で、0 なら影なし。`align` は `x` を左端・中央・右端のどれとするか。

#### `cyd.overlay_rect(x, y, width, height, color=WHITE, fill=True)` → `bool`
長方形をゲーム画面の上に重ねます。`fill=False` で枠線だけ。

#### `cyd.overlay_clear()`
このフレームのオーバーレイを消します。オーバーレイはスプライトとスコア行の上に描かれ、`cyd.game_frame()` でも消えます。
1 フレーム 24 個まで（文字と長方形の合計）。
