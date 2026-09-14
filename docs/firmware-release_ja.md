# ビルド済みファームウェア

[English](firmware-release_en.md) | 日本語

ファームウェアは、公開リポジトリの Releases に zip ファイル（`cyd-desktop-firmware-<バージョン>.zip`）として置きます。ビルド環境がなくても、この zip だけで書き込めます。

## 書き込む（利用者）

対応するのは **ILI9341 版の ESP32-2432S028（CYD）** だけです（[対応ハードウェア](../README.md#対応ハードウェア)）。

### zip の中身

| ファイル | 内容 |
| --- | --- |
| `bootloader.bin`、`partition-table.bin`、`micropython.bin` | 0x1000、0x8000、0x10000 に書く 3 つのイメージ |
| `cyd-desktop-<バージョン>-full.bin` | 上の 3 つを 1 本にまとめたイメージ。0x0 に書く。**設定領域（NVS）も消去する**ので新品の基板向け |
| `FLASHING.md` | 書き込み手順（日英） |
| `BUILD-INFO.json` | バージョン、ソースのコミット、MicroPython のコミット、ESP-IDF のバージョン、各ファイルの SHA-256 |
| `SHA256SUMS` | 各イメージの SHA-256 |
| `LICENSE`、`THIRD_PARTY_NOTICES*.md`、`licenses/` | 本体と第三者ソフトウェアのライセンス |

### 手順

1. esptool を入れます：`pip install esptool`
2. zip を展開し、ファイルが壊れていないことを確かめます。

   ```powershell
   Get-FileHash micropython.bin   # SHA256SUMS の値と比べる
   ```

3. USB で接続してポート（例 `COM6`）を確認し、他のシリアル接続を閉じます。
4. 基板の状態に合わせて書き込みます。

   | 状態 | コマンド |
   | --- | --- |
   | 新品、またはこれより前のファームウェアが入っている（タッチ補正と設定は残る） | `python -m esptool --chip esp32 --port COM6 --before default_reset write_flash 0x1000 bootloader.bin 0x8000 partition-table.bin 0x10000 micropython.bin` |
   | このファームウェアからの更新 | `python -m esptool --chip esp32 --port COM6 --before default_reset write_flash 0x10000 micropython.bin` |
   | 1 ファイルで書きたい（Web ブラウザの書き込みツールなど。**設定も消える**） | `cyd-desktop-<バージョン>-full.bin` を 0x0 に書く |

5. タッチ補正が保存されていない基板では、起動すると補正の十字が 3 か所に順に出ます。それぞれの中心をタップします。無線（Wi-Fi／Bluetooth）は Settings > WIRELESS で選びます（初期値は OFF）。

## リリースを作る（配布する人）

バイナリは、それを作ったソースと必ず対応させます。公開リポジトリの、変更のないチェックアウトからビルドしてください。

1. 公開リポジトリでバージョンを決め、`package.json`・`package-lock.json`・`firmware/cyd_desktop_shell/include/desktop_version.h`・`bridge/index.mjs` を揃えます（食い違うと `npm test` が失敗します）。コミットしてタグを付けます（例 `v0.0.1`）。
2. そのコミットを、**55 文字以内のパス**にサブモジュールごと取得し、[ビルド環境を一から作る](build_ja.md) の手順でビルドします。
3. ビルド結果をリポジトリのルートへコピーし、パッケージを作ります。

   ```powershell
   wsl -d Ubuntu-24.04 -e bash -c 'b=firmware/micropython/ports/esp32/build-cyd_desktop_board; cp "$b/micropython.bin" "$b/bootloader/bootloader.bin" "$b/partition_table/partition-table.bin" .'
   npm run package:firmware
   ```

   `dist/cyd-desktop-firmware-<バージョン>.zip` ができ、各ファイルの SHA-256 が表示されます。コピーした 3 つのバイナリと `dist/` は `.gitignore` の対象です。
   - 未コミットの変更や Git 未登録のファイルがあると、作成を拒否します（試しに作るだけなら `-- --allow-dirty`。その場合は `BUILD-INFO.json` の `source_dirty` が `true` になります）。
   - 1 本にまとめたイメージは esptool の `merge_bin` で作ります（`python -m esptool` が使える必要があります）。作れない環境では `-- --no-merged` で省けます。
4. 書き込んで動作を確かめます。少なくとも、新品相当の手順（3 ファイル）と更新の手順を試してください。
5. GitHub の Releases でタグのリリースを作り、zip を添付します。リリースノートには、バージョン、対応ハードウェア、表示された SHA-256、変更点を書きます。

### ライセンス資料を同梱する理由

ファームウェアには MicroPython（MIT）、ESP-IDF（Apache-2.0）、FreeRTOS（MIT）、lwIP（BSD）、newlib（BSD 系）などが含まれます。これらはバイナリを配布するときに著作権表示とライセンス文の添付を求めるため、パッケージには `LICENSE`、`THIRD_PARTY_NOTICES*.md`、`licenses/` を必ず入れています。依存やビルド設定を変えたときは、[第三者ライセンス資料](../THIRD_PARTY_NOTICES_ja.md) の一覧を見直してください。
