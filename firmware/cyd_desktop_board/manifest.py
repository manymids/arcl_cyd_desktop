# Match the essential ESP32 startup modules, but replace the port's _boot.py
# with the board-specific version below so the optional SD can be mounted.
freeze("$(PORT_DIR)/modules", ("apa106.py", "espnow.py", "flashbdev.py", "inisetup.py", "machine.py"))
freeze("modules", ("_boot.py", "_manifest.py", "cyd.py"))
