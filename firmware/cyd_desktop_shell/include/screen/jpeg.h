#pragma once

// JPEG decoding keeps a second band and a work area (about 14 KB) only while
// it is in use: a single image frees them when it is done, and the video
// surface keeps them until the compositor leaves that view.

namespace cyd::desktop::screen {

// Call with the display mutex held.
void jpeg_release_buffers();

}  // namespace cyd::desktop::screen
