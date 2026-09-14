#include <cassert>
#include "shell_state.h"

int main() {
    cyd::desktop::ShellState shell;
    assert(shell.snapshot().view == cyd::desktop::View::Home);
    assert(shell.dispatch(cyd::desktop::Action::OpenStart));
    assert(shell.snapshot().start_open);
    assert(shell.dispatch(cyd::desktop::Action::LaunchClock));
    assert(shell.snapshot().view == cyd::desktop::View::Clock);
    assert(shell.snapshot().foreground_app[0] == 'c');
    assert(shell.dispatch(cyd::desktop::Action::GoHome));
    assert(shell.snapshot().view == cyd::desktop::View::Home);
    assert(!shell.snapshot().start_open);
    assert(shell.launch_native("neon3d"));
    assert(shell.snapshot().view == cyd::desktop::View::Native);
    assert(shell.snapshot().foreground_app[0] == 'n');
    assert(shell.dispatch(cyd::desktop::Action::GoHome));
    return 0;
}
