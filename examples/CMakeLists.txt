# ==================================================================================================
# CoralSubtitle examples
# ==================================================================================================
# The framework's standalone demo. `subtitle_render_demo` builds a subtitle document (or reads one),
# parses it through the extractor's parser factory, rasterises the cues that are active at one time,
# and composites the overlay over a synthetic frame. It writes a PNG and, on macOS, presents the
# frame in a Cocoa window. That is the whole layer in one program: the document, the cue model, the
# renderer and the picture.
#
# Two ways to build it:
#
#   standalone, as its own project
#     cmake -S examples -B build/examples -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
#     cmake --build build/examples
#     ./build/examples/subtitle_render_demo
#
#   added by the framework root
#     A top-level configure of the repository adds this directory (see the root CMakeLists), so the
#     standalone build and the repository build share one target. A project that consumes CoralSubtitle
#     as a submodule does not get this executable.
#
# The demo needs no asset: the sample SubRip document is compiled into it. The dependency repositories
# have to be fetched first with scripts/bootstrap.sh (or scripts/bootstrap.ps1); see the header of the
# repository README.

set(CORALSUBTITLE_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")

if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  # Standalone. Resolve the dependencies and add the framework the same way the application root does:
  # the dependency graph first, then the framework, then the demo.
  cmake_minimum_required(VERSION 3.19)
  project(CoralSubtitleExamples LANGUAGES CXX)

  # CoralCore, CoralMedia and CoralGraphics carry Objective-C++ on Apple; the language has to be
  # enabled before the framework adds them.
  if (APPLE)
    enable_language(OBJCXX)
  endif ()

  set(IS_HOST_PLATFORM TRUE)

  # The demo links the framework's library; it does not build or run the unit suite, which is why this
  # project does not need GoogleTest even when the third-party tree carries it.
  set(CORAL_BUILD_TESTS OFF CACHE BOOL "Build the CoralSubtitle tests" FORCE)

  add_subdirectory("${CORALSUBTITLE_ROOT}" "${CMAKE_BINARY_DIR}/coral-subtitle")

  # The framework's CMakeLists defines this function. Calling it here gives the demo the same C++
  # standard and runtime flags as the library it links; this project is the parent of the framework in
  # this configure, so it does not inherit them through directory scope.
  coralsubtitle_apply_build_settings()
endif ()

add_executable(subtitle_render_demo subtitle_render_demo.cpp)
target_link_libraries(subtitle_render_demo PRIVATE subtitle)
set_target_properties(subtitle_render_demo PROPERTIES FOLDER Examples)

if (APPLE)
  # The window is the only platform-specific part of the demo: the bitmap is premultiplied RGBA8888,
  # which is an NSBitmapImageRep without bitmap flags, so presenting it is a row copy.
  target_sources(subtitle_render_demo PRIVATE platform/mac/SubtitleDemoWindow.mm)
  target_compile_options(subtitle_render_demo PRIVATE -fobjc-arc)
  find_library(COCOA_FRAMEWORK Cocoa REQUIRED)
  find_library(QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
  target_link_libraries(subtitle_render_demo PRIVATE
      ${COCOA_FRAMEWORK}
      ${QUARTZCORE_FRAMEWORK})
endif ()

enable_testing()
# The verification mode renders without a window, so a build machine (or CI) can run the demo. It
# fails when the frame holds no subtitle pixel, which is what would happen if the parser or the
# renderer broke.
add_test(NAME subtitle_render_demo_verify COMMAND subtitle_render_demo --verify)
set_tests_properties(subtitle_render_demo_verify PROPERTIES TIMEOUT 120)
