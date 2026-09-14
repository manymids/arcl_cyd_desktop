# Setting up the firmware build from scratch

English | [日本語](build_ja.md)

These steps set up WSL (Ubuntu 24.04) on Windows, install ESP-IDF, build the firmware
from the MicroPython included in this repository and flash it from Windows. For incremental builds in an existing
environment, see "Building and flashing firmware" in the [project README](../README_en.md).

## Why the versions are pinned

| Dependency | Version |
| --- | --- |
| ESP-IDF | v5.5.2 |
| MicroPython | v1.29.0 |

Both are used unmodified at those tags. MicroPython is included as a submodule at
`firmware/micropython`, whose commit is v1.29.0. The firmware does, however, replace these
internal functions at link time (`-Wl,--wrap` in
`firmware/cyd_desktop_board/components/cyd_bt_keyboard/CMakeLists.txt`):

- `gc_get_max_new_split` (MicroPython): limits GC heap growth while Bluetooth runs
- `esp_event_handler_instance_register` and `esp_now_init` (ESP-IDF): refuse to start
  Wi-Fi outside Wi-Fi radio mode

Other versions may drop these functions or call them differently, which either fails
to link or needs re-checking on the board. Keep the versions above.

## 1. WSL and packages

Install WSL from PowerShell (skip if it is already installed):

```powershell
wsl --install -d Ubuntu-24.04
```

In Ubuntu, install what ESP-IDF needs, plus g++ for the host tests:

```bash
sudo apt update
sudo apt install -y git wget flex bison gperf python3 python3-pip python3-venv cmake ninja-build ccache \
  libffi-dev libssl-dev dfu-util libusb-1.0-0 make g++
```

## 2. Install ESP-IDF and fetch MicroPython

Keep ESP-IDF and its tools in the WSL home. Setting `IDF_TOOLS_PATH` keeps the tools
apart from the default `~/.espressif`.

```bash
mkdir -p ~/cyd-desktop-vendor && cd ~/cyd-desktop-vendor
git clone -b v5.5.2 --recursive --depth 1 --shallow-submodules https://github.com/espressif/esp-idf.git
IDF_TOOLS_PATH=~/cyd-desktop-vendor/.idf-tools ./esp-idf/install.sh esp32
```

MicroPython comes from this repository's submodule. In the clone on the Windows side,
from PowerShell:

```powershell
git submodule update --init firmware/micropython
```

Do not add `--recursive`: MicroPython's own submodules cover many other boards, and the
two the ESP32 port needs are fetched by `make ... submodules` below. MicroPython pins
LF line endings in its `.gitattributes`, so a Windows checkout builds in WSL as it is.

## 3. First build

WSL sees this repository under `/mnt/<drive>/...`. Replace `REPO` below with your path.

**Keep the repository at a short path: 55 characters or fewer as seen from WSL (for
example `C:\src\cyd-desktop`, which is `/mnt/c/src/cyd-desktop`).** MicroPython's QSTR
generation puts every source and include path into one command, and with a long path it
exceeds the Linux argument limit (131,072 bytes) and fails with `Argument list too long`.
Configuring stops with this explanation when the path is longer than 55 characters.

```bash
export IDF_TOOLS_PATH=~/cyd-desktop-vendor/.idf-tools
. ~/cyd-desktop-vendor/esp-idf/export.sh
REPO=/mnt/c/src/cyd-desktop   # this repository as seen from WSL
MP=$REPO/firmware/micropython

make -C $MP/mpy-cross
make -C $MP/ports/esp32 BOARD_DIR=$REPO/firmware/cyd_desktop_board submodules

cd $MP/ports/esp32
idf.py -D MICROPY_BOARD=cyd_desktop_board \
  -D MICROPY_BOARD_DIR=$REPO/firmware/cyd_desktop_board \
  -D USER_C_MODULES=$REPO/firmware/cyd_desktop_shell/micropython.cmake \
  -B build-cyd_desktop_board build
```

Later builds only need the incremental command in the [project README](../README_en.md);
the build directory remembers the `-D` options. After changing the board definition
(`mpconfigboard.cmake`, `sdkconfig.bt`), delete
`firmware/micropython/ports/esp32/build-cyd_desktop_board/sdkconfig` first. MicroPython's
`.gitignore` covers build directories, so the submodule does not show as modified.

Building on a Windows drive is slower than in the WSL home (the first build takes from a
few minutes to over ten).

## 4. Flash

From a Windows shell at the repository root, copy the build output and flash it. A
blank board also needs the bootloader and partition table; afterwards the app partition
at `0x10000` is enough. Install esptool with `pip install esptool`.

```powershell
wsl -d Ubuntu-24.04 -e bash -c 'b=firmware/micropython/ports/esp32/build-cyd_desktop_board; cp "$b/micropython.bin" "$b/bootloader/bootloader.bin" "$b/partition_table/partition-table.bin" .'
python -m esptool --port COM6 --before default_reset write_flash 0x1000 bootloader.bin 0x8000 partition-table.bin 0x10000 micropython.bin
```

Replace `COM6` with your port. On first boot, touch calibration shows a crosshair at
three places in turn; tap the centre of each (see [Supported hardware](../README_en.md#supported-hardware)).

## 5. Tests

```powershell
npm ci
npm test
npm run test:native
```

`npm run test:native` runs the C++ and Python checks with g++ in WSL. Check that both
`failed` and `skipped` are 0.
