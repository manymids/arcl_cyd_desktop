# MJP Movie

Streams a 240x240 MJP1 file from the SD card and sends one JPEG frame at a time
to the native ESP32 ROM decoder. The player holds neither a full RGB565 frame
buffer nor the complete frame index in RAM.

On launch, the app scans `/sd/movies` (two subdirectory levels), the SD root,
and the legacy `/sd/apps/mjpplayer` location for up to 30 `.mjp` or `.mjpg`
files. It presents them in a three-row paged picker. Files must contain the
indexed 240x240 MJP1 format. Long-press Home or tap the taskbar Home region to
exit playback.

Playback needs about 49 KB of free heap, so it does not work in Bluetooth radio
mode; choose OFF or WI-FI in Settings > WIRELESS.
