"""Host test for _manifest.py, the schema the device loader and launcher use.

Runs under CPython: _manifest.py deliberately depends on nothing MicroPython
specific. The cases are shared with bridge/manifest.test.mjs so the PC-side
validator cannot drift from the device.
"""

import json
import os
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "modules"))

import _manifest  # noqa: E402


def shared_cases():
    with open(os.path.join(HERE, "manifest-cases.json"), encoding="utf-8") as source:
        spec = json.load(source)
    assert spec["api_level"] == _manifest.API_LEVEL, (
        "manifest-cases.json api_level %d != _manifest.API_LEVEL %d"
        % (spec["api_level"], _manifest.API_LEVEL))
    failures = []
    for case in spec["cases"]:
        try:
            title, entry = _manifest.validate(case["app_id"], case["manifest"])
            outcome = ("ok", title, entry)
        except ValueError as error:
            outcome = ("error", str(error).split(":", 1)[0])
        if case["ok"]:
            expected = ("ok", case["title"], case["entry"])
        else:
            expected = ("error", case["field"])
        if outcome != expected:
            failures.append("%s: expected %s, got %s" % (case["name"], expected, outcome))
    assert not failures, "\n".join(failures)
    return len(spec["cases"])


def load_reads_the_installed_file():
    with tempfile.TemporaryDirectory() as root:
        os.mkdir(os.path.join(root, "good"))
        with open(os.path.join(root, "good", "manifest.json"), "w") as output:
            json.dump({"title": "GOOD", "entry": "app.py"}, output)
        assert _manifest.load("good", root) == ("GOOD", "app.py")

        os.mkdir(os.path.join(root, "bare"))
        assert _manifest.load("bare", root) == ("bare", "main.py")

        os.mkdir(os.path.join(root, "broken"))
        with open(os.path.join(root, "broken", "manifest.json"), "w") as output:
            output.write("{ not json")
        try:
            _manifest.load("broken", root)
            raise AssertionError("broken JSON accepted")
        except ValueError as error:
            assert str(error).startswith("json:")

        os.mkdir(os.path.join(root, "huge"))
        with open(os.path.join(root, "huge", "manifest.json"), "w") as output:
            output.write('{"title": "X", "version": "' + "1" * _manifest.MAX_BYTES + '"}')
        try:
            _manifest.load("huge", root)
            raise AssertionError("oversized manifest accepted")
        except ValueError as error:
            assert str(error).startswith("shape:")


def messages_fit_the_app_log():
    # _load_apps logs "skip <id>: <error>" into a 48-character line. Keep the
    # field name visible even for a 24-character app id.
    worst = None
    for data in ({"tittle": "X" * 40}, {"title": "X", "api": 99}, {"title": "!"}):
        try:
            _manifest.validate("x" * 24, data)
        except ValueError as error:
            line = "skip %s: %s" % ("x" * 24, error)
            field = str(error).split(":", 1)[0]
            assert field in line[:48], line
            worst = line
    assert worst is not None


if __name__ == "__main__":
    count = shared_cases()
    load_reads_the_installed_file()
    messages_fit_the_app_log()
    print("manifest_test: %d shared cases and load/log checks passed" % count)
