# What one app run may leave behind in the interpreter, and how to undo it.
#
# Apps share one MicroPython heap and one sys.modules. An app that imports its
# own module (examples/pip_quest and examples/ember_vale both "import game")
# would otherwise hand that cached module to the next app that imports the
# same name, and run the wrong code. Pure Python, so the host can test it.
import sys


def begin():
    """Record the modules and import path in place before an app starts."""
    return (set(sys.modules), list(sys.path))


def end(snapshot):
    """Forget every module the app imported and restore its import path."""
    modules, path = snapshot
    for name in [name for name in sys.modules if name not in modules]:
        del sys.modules[name]
    sys.path.clear()
    sys.path.extend(path)
