set(SDKCONFIG_DEFAULTS boards/sdkconfig.base)
set(MICROPY_FROZEN_MANIFEST ${MICROPY_BOARD_DIR}/manifest.py)

# MicroPython collects QSTRs with a single shell command naming every source
# file and include directory. Linux refuses one argument over 131,072 bytes
# ("Argument list too long"), and this repository's path appears in that
# command about 720 times: measured, a 123-character path made it 176,779
# bytes. Stop at configure time with the reason rather than minutes later.
get_filename_component(CYD_REPOSITORY_ROOT "${MICROPY_BOARD_DIR}/../.." ABSOLUTE)
string(LENGTH "${CYD_REPOSITORY_ROOT}" CYD_REPOSITORY_ROOT_LENGTH)
if(CYD_REPOSITORY_ROOT_LENGTH GREATER 55)
    message(FATAL_ERROR
        "The repository path ${CYD_REPOSITORY_ROOT} is ${CYD_REPOSITORY_ROOT_LENGTH} characters long. "
        "Move the repository to a path of 55 characters or fewer, such as /mnt/c/src/cyd-desktop "
        "or ~/cyd-desktop: with a longer one, MicroPython's QSTR generation exceeds the Linux "
        "argument length limit and fails with \"Argument list too long\".")
endif()

# Bluetooth Classic keyboards. The radio mode in Settings > Wireless chooses at
# boot between Wi-Fi, Bluetooth and neither; the Bluetooth stack's memory is
# released to the heap unless Bluetooth is selected.
#
# Bluedroid and the HID host add about 600 KB of code, so the app partition is
# 3 MB instead of MicroPython's 2 MB. The partition table setting is written
# into the build directory because sdkconfig cannot refer to this board
# directory. Changing partition tables takes a full flash (see README).
set(CYD_DESKTOP_BLUETOOTH ON)
list(APPEND EXTRA_COMPONENT_DIRS ${MICROPY_BOARD_DIR}/components)
# Bluetooth needs about 80 KB of the heap, so the MicroPython heap starts at
# 32 KB instead of 56 KB and grows when an app needs more.
list(APPEND MICROPY_DEF_BOARD MICROPY_GC_INITIAL_HEAP_SIZE=32768)

file(WRITE ${CMAKE_BINARY_DIR}/sdkconfig.cyd_partitions
    "CONFIG_PARTITION_TABLE_CUSTOM=y\n"
    "CONFIG_PARTITION_TABLE_CUSTOM_FILENAME=\"${MICROPY_BOARD_DIR}/partitions-cyd.csv\"\n")

list(APPEND SDKCONFIG_DEFAULTS
    ${MICROPY_BOARD_DIR}/sdkconfig.bt
    ${CMAKE_BINARY_DIR}/sdkconfig.cyd_partitions)
