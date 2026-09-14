# ファームウェアのビルド環境を一から作る

[English](build_en.md) | 日本語

Windows に WSL（Ubuntu 24.04）を用意し、ESP-IDF を導入して、このリポジトリに同梱した MicroPython からファームウェアをビルドし、Windows 側から書き込むまでの手順です。
すでに環境がある場合の増分ビルドと書き込みは、[プロジェクトREADME](../README.md) の「ファームウェアのビルドと書込み」を参照してください。

## バージョンを固定する理由

| 依存元 | バージョン |
| --- | --- |
| ESP-IDF | v5.5.2 |
| MicroPython | v1.29.0 |

どちらもタグのまま、変更を加えずに使います。MicroPython は `firmware/micropython` にサブモジュールとして同梱しており、そのコミットが v1.29.0 です。ただし、このファームウェアは次の内部関数をリンク時に差し替えています（`firmware/cyd_desktop_board/components/cyd_bt_keyboard/CMakeLists.txt` の `-Wl,--wrap`）。

- `gc_get_max_new_split`（MicroPython）：Bluetooth 動作中に GC ヒープの拡張量を制限する
- `esp_event_handler_instance_register`、`esp_now_init`（ESP-IDF）：Wi-Fi モード以外で Wi-Fi の起動を拒否する

バージョンを変えるとこれらの関数が無くなったり、呼び出され方が変わったりすることがあります。リンクエラーになるか、実機での確認が必要になるので、上のバージョンから変えないでください。

## 1. WSL と必要なパッケージ

PowerShell で WSL を入れます（入れ済みなら不要）。

```powershell
wsl --install -d Ubuntu-24.04
```

Ubuntu 側で、ESP-IDF のビルドに必要なパッケージと、ホスト側テスト用の g++ を入れます。

```bash
sudo apt update
sudo apt install -y git wget flex bison gperf python3 python3-pip python3-venv cmake ninja-build ccache \
  libffi-dev libssl-dev dfu-util libusb-1.0-0 make g++
```

## 2. ESP-IDF の導入と MicroPython の取得

ESP-IDF とツールは WSL のホームに置きます。ツールの置き場所を `IDF_TOOLS_PATH` で固定すると、既定の `~/.espressif` と混ざりません。

```bash
mkdir -p ~/cyd-desktop-vendor && cd ~/cyd-desktop-vendor
git clone -b v5.5.2 --recursive --depth 1 --shallow-submodules https://github.com/espressif/esp-idf.git
IDF_TOOLS_PATH=~/cyd-desktop-vendor/.idf-tools ./esp-idf/install.sh esp32
```

MicroPython は、このリポジトリのサブモジュールを取得します。Windows 側で clone したリポジトリで、PowerShell から実行します。

```powershell
git submodule update --init firmware/micropython
```

`--recursive` は付けないでください。MicroPython 自身のサブモジュールは他のボード用も含めて大量にあり、ESP32 に必要な 2 つは次の手順の `make ... submodules` が取得します。MicroPython は `.gitattributes` で改行を LF に固定しているので、Windows で取得しても WSL でそのままビルドできます。

## 3. 初回のビルド

WSL からは、このリポジトリが `/mnt/<ドライブ>/...` で見えます。以下の `REPO` を自分のパスに置き換えてください。

**リポジトリは短いパスに置いてください（WSL から見て 55 文字以内。例：`C:\src\cyd-desktop` → `/mnt/c/src/cyd-desktop`）。** MicroPython の QSTR 生成は、すべてのソースとインクルードのパスを 1 本のコマンドに並べるため、パスが長いと Linux の引数長の上限（131,072 バイト）を超えて `Argument list too long` で失敗します。55 文字を超えるとビルドの構成時に理由を表示して止まります。

```bash
export IDF_TOOLS_PATH=~/cyd-desktop-vendor/.idf-tools
. ~/cyd-desktop-vendor/esp-idf/export.sh
REPO=/mnt/c/src/cyd-desktop   # このリポジトリの WSL から見たパス
MP=$REPO/firmware/micropython

make -C $MP/mpy-cross
make -C $MP/ports/esp32 BOARD_DIR=$REPO/firmware/cyd_desktop_board submodules

cd $MP/ports/esp32
idf.py -D MICROPY_BOARD=cyd_desktop_board \
  -D MICROPY_BOARD_DIR=$REPO/firmware/cyd_desktop_board \
  -D USER_C_MODULES=$REPO/firmware/cyd_desktop_shell/micropython.cmake \
  -B build-cyd_desktop_board build
```

2 回目以降は、`-D` の指定はビルドディレクトリに記録されているので、[プロジェクトREADME](../README.md) の増分ビルドのコマンドで足ります。ボード定義（`mpconfigboard.cmake` や `sdkconfig.bt`）を変えたときは、`firmware/micropython/ports/esp32/build-cyd_desktop_board/sdkconfig` を消してから実行してください。ビルドディレクトリは MicroPython の `.gitignore` の対象なので、サブモジュールは変更扱いになりません。

Windows のドライブ上でのビルドは、WSL のホーム上より遅くなります（初回はおおむね数分〜十数分）。

## 4. 書き込み

Windows 側でこのリポジトリのルートを開き、ビルド結果をコピーして書き込みます。未書込みの基板では bootloader とパーティションテーブルも書きます（2 回目以降はアプリ領域の `0x10000` だけで足ります）。esptool は `pip install esptool` で入ります。

```powershell
wsl -d Ubuntu-24.04 -e bash -c 'b=firmware/micropython/ports/esp32/build-cyd_desktop_board; cp "$b/micropython.bin" "$b/bootloader/bootloader.bin" "$b/partition_table/partition-table.bin" .'
python -m esptool --port COM6 --before default_reset write_flash 0x1000 bootloader.bin 0x8000 partition-table.bin 0x10000 micropython.bin
```

`COM6` は実際のポートに置き換えてください。初回起動では、タッチ補正の十字が 3 か所に順に表示されるので、それぞれの中心をタップします（[対応ハードウェア](../README.md#対応ハードウェア)）。

## 5. テスト

```powershell
npm ci
npm test
npm run test:native
```

`npm run test:native` は WSL の g++ で C++ と Python の検証を実行します。結果の `failed` と `skipped` がどちらも 0 であることを確認してください。
