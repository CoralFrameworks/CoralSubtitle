/*
 * Copyright (C) 2026 The Coral Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef CORAL_SUBTITLE_EXAMPLES_SUBTITLE_DEMO_WINDOW_H
#define CORAL_SUBTITLE_EXAMPLES_SUBTITLE_DEMO_WINDOW_H

#include <string>

namespace graphics {
class Bitmap;
} // namespace graphics

namespace coral {
namespace demo {

#if defined(__APPLE__)

/**
 * Presents @p frame in a Cocoa window and blocks until the user closes it or @p seconds elapses.
 *
 * <p>This is the macOS half of the demo: the renderer and the compositor are platform independent, and
 * the only thing this file adds is the window that shows the result. The view scales the frame to fit
 * and keeps its aspect ratio; Esc, Q, the red button and Cmd+Q all end the loop.
 *
 * @param frame The composited RGBA frame, in the premultiplied layout of `graphics::Bitmap`.
 * @param seconds Auto-close after this many seconds, or 0 to wait for the user.
 * @param title The window title, usually the document name and the frame time.
 *
 * @return false when the application or the window could not be created.
 */
[[nodiscard]] bool presentSubtitleFrame(const graphics::Bitmap& frame, double seconds,
                                        const std::string& title);

#endif // __APPLE__

} // namespace demo
} // namespace coral

#endif //CORAL_SUBTITLE_EXAMPLES_SUBTITLE_DEMO_WINDOW_H
