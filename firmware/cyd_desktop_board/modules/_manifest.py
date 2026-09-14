"""The manifest.json schema for /sd/apps/<app-id>/.

One definition, used by the loader and the launcher. The same rules are
implemented for the PC side in bridge/manifest.mjs; both are checked against
firmware/cyd_desktop_board/tests/manifest-cases.json, and the schema is
documented in docs/cyd-api.md.

    {
      "id":      "weather",   optional; must equal the directory name
      "title":   "WEATHER",   required; 1-12 of A-Z a-z 0-9 space _ . -
      "entry":   "main.py",   optional; the .py file to run
      "api":     2,           optional; the cyd API level the app needs
      "version": "0.0.1"      optional; informational, up to 16 characters
    }

An app directory without manifest.json is valid: its title is the app id and
its entry is main.py. Errors are raised as ValueError("<field>: <reason>") so
callers can report which field was wrong.
"""

import json

# Bump when cyd gains functions an app may depend on. An app whose manifest asks
# for a higher level is refused instead of failing halfway through with an
# AttributeError.
API_LEVEL = 4

MAX_BYTES = 2048
KEYS = ("id", "title", "entry", "api", "version")


def safe_component(value, maximum=24):
    if not isinstance(value, str) or not value or len(value) > maximum or ".." in value:
        return False
    for character in value:
        if not (("a" <= character <= "z") or ("A" <= character <= "Z")
                or ("0" <= character <= "9") or character in "_.-"):
            return False
    return True


def safe_title(value):
    if not isinstance(value, str) or not value or len(value) > 12:
        return False
    for character in value:
        if not (("a" <= character <= "z") or ("A" <= character <= "Z")
                or ("0" <= character <= "9") or character in " _.-"):
            return False
    return True


def validate(app_id, data):
    """Return (title, entry) for an app, or raise ValueError("<field>: <reason>")."""
    if data is None:
        return app_id[:12], "main.py"
    if not isinstance(data, dict):
        raise ValueError("shape: manifest must be a JSON object")
    for key in data:
        if key not in KEYS:
            # Most often a typo, such as "tittle", that would otherwise be ignored.
            raise ValueError("key: unknown key " + str(key)[:16])
    if "id" in data and data["id"] != app_id:
        raise ValueError("id: does not match the directory name")
    title = data.get("title")
    if not safe_title(title):
        raise ValueError("title: need 1-12 of A-Z 0-9 space _.-")
    entry = data.get("entry", "main.py")
    if not safe_component(entry) or not entry.endswith(".py"):
        raise ValueError("entry: must be a .py file name")
    api = data.get("api", 1)
    if isinstance(api, bool) or not isinstance(api, int) or api < 1:
        raise ValueError("api: must be a positive integer")
    if api > API_LEVEL:
        raise ValueError("api: needs %d, firmware has %d" % (api, API_LEVEL))
    version = data.get("version")
    if version is not None and not (isinstance(version, str) and 0 < len(version) <= 16):
        raise ValueError("version: must be 1-16 characters")
    return title, entry


def load(app_id, root="/sd/apps"):
    """Read and validate an installed app's manifest."""
    path = root + "/" + app_id + "/manifest.json"
    try:
        with open(path, "r") as source:
            raw = source.read(MAX_BYTES + 1)
    except OSError:
        return validate(app_id, None)
    if len(raw) > MAX_BYTES:
        raise ValueError("shape: larger than %d bytes" % MAX_BYTES)
    try:
        data = json.loads(raw)
    except ValueError:
        raise ValueError("json: manifest.json is not valid JSON")
    return validate(app_id, data)
