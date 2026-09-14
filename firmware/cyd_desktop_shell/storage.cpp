// ENODEV is the neutral pre-boot state; actual mount status comes from the
// MicroPython boot hook below.
#include <cerrno>

namespace {

int last_error = ENODEV;
int boot_error = ENODEV;

}  // namespace

// Reports the mount performed at boot; it does not mount anything itself.
// /sd belongs to MicroPython's VFS, not the ESP-IDF POSIX VFS, so the frozen
// _boot.py does the mount and directory setup and records the result through
// cyd_desktop_sd_set_boot_error(). A card inserted after boot needs a restart,
// which is why desktop_sd_mount cannot bring one online.
extern "C" bool cyd_desktop_sd_boot_mount_succeeded(void) {
    last_error = boot_error;
    return boot_error == 0;
}

extern "C" bool cyd_desktop_sd_mount(void) { return cyd_desktop_sd_boot_mount_succeeded(); }

extern "C" bool cyd_desktop_sd_mounted(void) { return cyd_desktop_sd_mount(); }
extern "C" int cyd_desktop_sd_last_error(void) { return last_error; }
extern "C" int cyd_desktop_sd_boot_error(void) { return boot_error; }
extern "C" void cyd_desktop_sd_set_boot_error(int error) { boot_error = error; }

extern "C" void cyd_desktop_sd_unmount(void) {}
