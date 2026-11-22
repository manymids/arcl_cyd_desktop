"""Host test for _app_scope.py, which _boot.py wraps around every app run.

Reproduces the device bug where PIP QUEST then EMBER VALE (both "import game")
ran PIP QUEST's game module the second time, because the first app's import
was still cached in sys.modules.
"""

import os
import shutil
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "modules"))

import _app_scope  # noqa: E402


def make_app(root, app_id, marker):
    directory = os.path.join(root, app_id)
    os.mkdir(directory)
    with open(os.path.join(directory, "game.py"), "w", encoding="utf-8") as target:
        target.write("NAME = %r\n" % marker)
    return directory


def run_app(directory):
    """What an app's main.py does: put its folder on the path and import game."""
    scope = _app_scope.begin()
    try:
        sys.path.insert(0, directory)
        import game
        return game.NAME
    finally:
        _app_scope.end(scope)


def main():
    root = tempfile.mkdtemp()
    try:
        first = make_app(root, "pipquest", "PIP QUEST")
        second = make_app(root, "embervale", "EMBER VALE")
        path_before = list(sys.path)
        modules_before = set(sys.modules)

        assert run_app(first) == "PIP QUEST"
        assert "game" not in sys.modules, "the app's module outlived the app"
        assert sys.path == path_before, "the app's sys.path entry outlived the app"
        assert run_app(second) == "EMBER VALE", "second app ran the first app's module"
        assert run_app(first) == "PIP QUEST"

        # Modules loaded before the app (cyd on the device) are kept.
        assert modules_before <= set(sys.modules)
        assert "_app_scope" in sys.modules
    finally:
        shutil.rmtree(root)
    print("app_scope: ok")


if __name__ == "__main__":
    main()
