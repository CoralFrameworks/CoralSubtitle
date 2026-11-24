# ==================================================================================================
# CoralSubtitle
# ==================================================================================================
# The subtitle layer between the extractor's cue model and the Graphics Canvas. It rasterises the cues
# of one frame into an RGBA overlay, which a composition renderer or a player blends over the picture;
# it does not depend on Playback, Compose or UI.
#
# The repository is both a standalone project and the source of the `subtitle` library an application
# links. The public headers live in include/subtitle/ (included as <subtitle/...>), the implementation
# in src/. CoralAudio keeps its public headers under coral/audio/; this framework keeps the
# `subtitle/` prefix its consumers already compile against.
#
# The dependency repositories are declared in .gitmodules but are not part of this checkout, so a
# standalone build fetches them first with the bootstrap tool for the host:
#
#   macOS / Linux:  ./scripts/bootstrap.sh
#   Windows:        .\scripts\bootstrap.ps1
#
# then, on every platform:
#
#   cmake -S . -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release   # macOS / Linux
#   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release                        # Windows (Visual Studio)
#   cmake --build build
#   ctest --test-dir build -R "test_Subtitle|subtitle_render_demo_verify" --output-on-failure
#
# The demo project in examples/ is added by a top-level configure and can also be configured on its
# own: `cmake -S examples -B build/examples`.

cmake_minimum_required(VERSION 3.19)
project(CoralSubtitle LANGUAGES CXX)

# CoralCore, CoralMedia and CoralGraphics carry Objective-C++ platform code on Apple: the media codecs
# and the graphics frame clock are written against platform classes. The language has to be enabled
# here, before those projects are added, so that a subproject's `enable_language(OBJCXX)` does not end
# up in a child directory scope where the parent's generate step cannot see its compile rule.
if (APPLE)
  enable_language(OBJCXX)
endif ()

# ==================================================================================================
# Build settings
# ==================================================================================================
# These settings apply to CoralSubtitle and, through directory scope, to the dependencies added below.
# The standalone examples project is the exception: it is the parent directory there, so it calls
# `coralsubtitle_apply_build_settings()` after adding this repository.
#
# Keep the block free of paths and third-party discovery: those are the concerns of the sections
# below, while this part is the C++ standard, the MSVC runtime and the warning baseline.
function(coralsubtitle_apply_build_settings)
  option(USE_STATIC_CRT "Link against the static runtime libraries." ON)

  if (MSVC)
    if (USE_STATIC_CRT)
      add_compile_options(
          $<$<CONFIG:>:/MT>
          $<$<CONFIG:Debug>:/MTd>
          $<$<CONFIG:Release>:/MT>
      )
    else ()
      add_compile_options(
          $<$<CONFIG:>:/MD>
          $<$<CONFIG:Debug>:/MDd>
          $<$<CONFIG:Release>:/MD>
      )
    endif ()
  endif ()

  # Windows: generate the .def that makes a DLL import library. CL.EXE does not produce one from
  # __declspec(dllexport) alone in every configuration.
  if (WIN32)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON PARENT_SCOPE)
  endif ()

  set(CMAKE_CXX_STANDARD 20 PARENT_SCOPE)
  set(CMAKE_CXX_STANDARD_REQUIRED ON PARENT_SCOPE)

  if (MSVC)
    # The project builds with C++20 (set above); keep the historical /W0 and the __cplusplus value
    # that libraries such as nlohmann/json check.
    set(CMAKE_CXX_FLAGS
        "${CMAKE_CXX_FLAGS} /W0 /Zc:__cplusplus"
        PARENT_SCOPE)
  else ()
    set(CMAKE_CXX_FLAGS
        "${CMAKE_CXX_FLAGS} -fstrict-aliasing -Wno-unknown-pragmas -Wno-unused-function -Wno-deprecated-declarations"
        PARENT_SCOPE)
    if (APPLE)
      # The platform headers are full of nullability annotations that the C++ mode does not need, and
      # the Objective-C++ translation units of the dependencies compile with the same warning set.
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-nullability-extension" PARENT_SCOPE)
      set(CMAKE_OBJCXX_FLAGS "${CMAKE_CXX_FLAGS}" PARENT_SCOPE)
    endif ()
  endif ()

  if (WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_USE_MATH_DEFINES=1" PARENT_SCOPE)
  endif ()
endfunction()

coralsubtitle_apply_build_settings()

# `subtitle` is one target; this switch decides whether it is libcoral-subtitle.a or
# libcoral-subtitle.so (libcoral-subtitle.dylib on macOS, coral-subtitle.dll on Windows). A shared
# CoralSubtitle links the framework's static dependencies, so they have to be position independent as
# well; the setting is applied before they are added.
option(BUILD_SHARED_LIBS "Build the CoralSubtitle library as a shared library" OFF)
if (BUILD_SHARED_LIBS)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif ()

# Indicates whether CoralSubtitle is a standalone project.
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  set(CORAL_SUBTITLE_TOP_LEVEL ON)
else ()
  set(CORAL_SUBTITLE_TOP_LEVEL OFF)
endif ()

if (CORAL_SUBTITLE_TOP_LEVEL)
  if (NOT DEFINED IS_HOST_PLATFORM)
    set(IS_HOST_PLATFORM TRUE)
  endif ()
  set(_coralsubtitle_tests_default ON)
else ()
  set(_coralsubtitle_tests_default OFF)
endif ()
# CORAL_BUILD_TESTS is the public switch: a caller's -DCORAL_BUILD_TESTS=OFF wins even when this is a
# standalone configure. As a subproject the default is off, because only this repository's own
# top-level build is expected to run its unit suite.
option(CORAL_BUILD_TESTS "Build the CoralSubtitle tests" ${_coralsubtitle_tests_default})
unset(_coralsubtitle_tests_default)
if (CORAL_BUILD_TESTS)
  enable_testing()
endif ()

# macOS deployment target. CoralGraphics uses CADisplayLink, which the SDK declares as available from
# macOS 14, and CoralCore's Minilog uses libc++ std::format, which needs 13.3. A standalone configure
# therefore refuses a target older than 14 with a warning that names the cause; the application tree
# keeps its own value untouched.
if (APPLE AND NOT IOS AND CORAL_SUBTITLE_TOP_LEVEL AND CMAKE_OSX_DEPLOYMENT_TARGET)
  if (CMAKE_OSX_DEPLOYMENT_TARGET VERSION_LESS 14.0)
    message(WARNING
        "CoralSubtitle requires a macOS deployment target of 14.0 or newer: CoralGraphics' Metal "
        "frame clock uses CADisplayLink, which the SDK declares as available from macOS 14. "
        "Configure with -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 or newer.")
  endif ()
endif ()

# ==================================================================================================
# Shared third-party tree
# ==================================================================================================
# GoogleTest is the only third-party library this repository adds itself, and it is test-only: a
# checkout or IDE profile that did not initialize it still has to be able to configure and build the
# library, so the tests are disabled with a warning instead of stopping the configure.
#
# The tree is resolved for a top-level configure only. A standalone CoralSubtitle checkout keeps its
# own third_party/ submodules; the application tree keeps them two levels above frameworks/CoralSubtitle;
# CoralGraphics and CoralCore carry their own copies under external/.
if (CORAL_SUBTITLE_TOP_LEVEL)
  # zlib is the probe rather than GoogleTest: CoralMedia needs it whether or not the tests are built,
  # while GoogleTest is only needed by the suite.
  if (NOT DEFINED CORAL_SHARED_THIRD_PARTY_DIR)
    foreach (_coral_third_party_candidate
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party"
        "${CMAKE_CURRENT_SOURCE_DIR}/external/CoralGraphics/third_party"
        "${CMAKE_CURRENT_SOURCE_DIR}/external/CoralCore/third_party")
      if (EXISTS "${_coral_third_party_candidate}/libz/coral/CMakeLists.txt")
        set(CORAL_SHARED_THIRD_PARTY_DIR "${_coral_third_party_candidate}")
        break()
      endif ()
    endforeach ()
  endif ()
  if (DEFINED CORAL_SHARED_THIRD_PARTY_DIR)
    message(STATUS "CoralSubtitle third-party tree: ${CORAL_SHARED_THIRD_PARTY_DIR}")
  else ()
    message(FATAL_ERROR
        "CoralSubtitle's third-party tree (libz, libyuv, libgtest) was not found. Run "
        "./scripts/bootstrap.sh (or .\\scripts\\bootstrap.ps1), pass "
        "-DCORAL_SHARED_THIRD_PARTY_DIR=<third_party>, or add the `z` target before CoralSubtitle.")
  endif ()

  if (CORAL_BUILD_TESTS AND NOT TARGET gtest)
    if (DEFINED CORAL_SHARED_THIRD_PARTY_DIR AND
        EXISTS "${CORAL_SHARED_THIRD_PARTY_DIR}/libgtest/coral/CMakeLists.txt")
      add_subdirectory("${CORAL_SHARED_THIRD_PARTY_DIR}/libgtest/coral" "${CMAKE_BINARY_DIR}/gtest")
    else ()
      message(WARNING
          "GoogleTest was not found, so the CoralSubtitle tests are disabled. Run "
          "./scripts/bootstrap.sh (or .\\scripts\\bootstrap.ps1) to fetch third_party/libgtest, "
          "pass -DCORAL_SHARED_THIRD_PARTY_DIR=<third_party>, or configure with "
          "-DCORAL_BUILD_TESTS=OFF.")
      set(CORAL_BUILD_TESTS OFF)
    endif ()
  endif ()
endif ()

# ==================================================================================================
# Dependencies
# ==================================================================================================
# CoralSubtitle needs all three lower frameworks. The application adds them in dependency order; a
# standalone checkout resolves the sibling repositories (the bootstrap tool clones them under
# external/) here. Each framework's own dependency manifest is guarded by `NOT TARGET`, so a parent
# tree that already added them is left alone.
#
# The dependency frameworks derive their test switches from CORAL_BUILD_TESTS, and Minilog's suite
# defaults on whenever IS_HOST_PLATFORM is exported. Their suites are not this repository's: a
# standalone CoralSubtitle build compiles its own tests only, which is why the shared switch is turned
# off for the add_subdirectory() calls and restored afterwards. A subproject does not do this — the
# application's value then applies to every framework it adds.
if (CORAL_SUBTITLE_TOP_LEVEL)
  set(_coralsubtitle_build_tests ${CORAL_BUILD_TESTS})
  set(CORAL_BUILD_TESTS OFF)
  set(MINILOG_BUILD_TESTS OFF)
endif ()

if (NOT TARGET utils)
  if (DEFINED CORAL_CORE_DIR AND EXISTS "${CORAL_CORE_DIR}/CMakeLists.txt")
    add_subdirectory("${CORAL_CORE_DIR}" "${CMAKE_BINARY_DIR}/coral-core")
  elseif (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/CoralCore/CMakeLists.txt")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/external/CoralCore"
        "${CMAKE_BINARY_DIR}/coral-core")
  elseif (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../CoralCore/CMakeLists.txt")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../CoralCore" "${CMAKE_BINARY_DIR}/coral-core")
  else ()
    message(FATAL_ERROR
        "CoralSubtitle needs CoralCore for `utils` and `minilog`. Run scripts/bootstrap.sh, pass "
        "-DCORAL_CORE_DIR=<path>, or add CoralCore before CoralSubtitle.")
  endif ()
endif ()

# CoralMedia provides `extractor`: the cue model (AVCue/AVCueGroup) and the subtitle parsers.
if (NOT TARGET extractor)
  if (DEFINED CORAL_MEDIA_DIR AND EXISTS "${CORAL_MEDIA_DIR}/CMakeLists.txt")
    add_subdirectory("${CORAL_MEDIA_DIR}" "${CMAKE_BINARY_DIR}/coral-media")
  elseif (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/CoralMedia/CMakeLists.txt")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/external/CoralMedia"
        "${CMAKE_BINARY_DIR}/coral-media")
  elseif (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../CoralMedia/CMakeLists.txt")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../CoralMedia" "${CMAKE_BINARY_DIR}/coral-media")
  else ()
    message(FATAL_ERROR
        "CoralSubtitle needs CoralMedia for the `extractor` cue model. Run scripts/bootstrap.sh, pass "
        "-DCORAL_MEDIA_DIR=<path>, or add CoralMedia before CoralSubtitle.")
  endif ()
endif ()

# CoralGraphics provides `graphics`: the Canvas, Bitmap, Font and Typeface the renderer draws with.
if (NOT TARGET graphics)
  if (DEFINED CORAL_GRAPHICS_DIR AND EXISTS "${CORAL_GRAPHICS_DIR}/CMakeLists.txt")
    add_subdirectory("${CORAL_GRAPHICS_DIR}" "${CMAKE_BINARY_DIR}/coral-graphics")
  elseif (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/CoralGraphics/CMakeLists.txt")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/external/CoralGraphics"
        "${CMAKE_BINARY_DIR}/coral-graphics")
  elseif (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../CoralGraphics/CMakeLists.txt")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../CoralGraphics"
        "${CMAKE_BINARY_DIR}/coral-graphics")
  else ()
    message(FATAL_ERROR
        "CoralSubtitle needs CoralGraphics for `graphics`. Run scripts/bootstrap.sh, pass "
        "-DCORAL_GRAPHICS_DIR=<path>, or add CoralGraphics before CoralSubtitle.")
  endif ()
endif ()

# Restore this repository's own test switch; see the note above the dependency blocks.
if (CORAL_SUBTITLE_TOP_LEVEL)
  set(CORAL_BUILD_TESTS ${_coralsubtitle_build_tests})
  unset(_coralsubtitle_build_tests)
endif ()

# ==================================================================================================
# CoralSubtitle library
# ==================================================================================================
# The target is named `subtitle`, which is what the rest of Coral links; the artifact is named
# `coral-subtitle` so the installed library is libcoral-subtitle.a (or .so/.dylib/.dll).
set(CORAL_SUBTITLE_TARGET subtitle)
set(CORAL_SUBTITLE_PUBLIC_HDR_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

set(PUBLIC_HDRS
    include/subtitle/HorizontalTextLayout.h
    include/subtitle/SubtitleRenderer.h
    include/subtitle/SubtitleResources.h
    include/subtitle/SubtitleStyle.h
    include/subtitle/TextLayout.h
    include/subtitle/VerticalTextLayout.h
)

set(SRCS
    src/CueLayout.cpp
    src/HorizontalCuePainter.cpp
    src/HorizontalTextLayout.cpp
    src/SubtitlePainter.cpp
    src/SubtitleRenderer.cpp
    src/SubtitleResources.cpp
    src/TextLayout.cpp
    src/VerticalCuePainter.cpp
    src/VerticalTextLayout.cpp
)

set(PRIVATE_HDRS
    src/CueLayout.h
    src/HorizontalCuePainter.h
    src/SubtitlePainter.h
    src/Utf8.h
    src/VerticalCuePainter.h
)

# The private headers are found by the translation units of this target only. `Utf8.h` is included as
# "Utf8.h" from src/, so the directory itself is on the private include path; the public headers are
# included as <subtitle/...> and stay the only header path a client gets.
#
# No STATIC keyword: BUILD_SHARED_LIBS decides the artifact, which is what lets
# -DBUILD_SHARED_LIBS=ON produce libcoral-subtitle.dylib (or .so/.dll).
add_library(${CORAL_SUBTITLE_TARGET} ${PUBLIC_HDRS} ${PRIVATE_HDRS} ${SRCS})
set_target_properties(${CORAL_SUBTITLE_TARGET} PROPERTIES OUTPUT_NAME coral-subtitle)
set_target_properties(${CORAL_SUBTITLE_TARGET} PROPERTIES FOLDER Libs)

target_include_directories(${CORAL_SUBTITLE_TARGET}
    PUBLIC ${CORAL_SUBTITLE_PUBLIC_HDR_DIR}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# ==================================================================================================
# Dependencies of the library
# ==================================================================================================
# `extractor` brings the cue model and the parsers, `graphics` the Canvas and the font stack, and
# `minilog` only records diagnostics from the renderer, so it stays private.
target_link_libraries(${CORAL_SUBTITLE_TARGET} PUBLIC extractor)
target_link_libraries(${CORAL_SUBTITLE_TARGET} PUBLIC graphics)
target_link_libraries(${CORAL_SUBTITLE_TARGET} PRIVATE minilog)

# ==================================================================================================
# Installation
# ==================================================================================================
# A shared build installs the import library next to the static one and the DLL under bin/.
install(TARGETS ${CORAL_SUBTITLE_TARGET}
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin)
# The public headers are included as <subtitle/...>, so `subtitle/` is what goes under include/.
install(DIRECTORY ${CORAL_SUBTITLE_PUBLIC_HDR_DIR}/subtitle DESTINATION include)

# ==================================================================================================
# Tests
# ==================================================================================================
if (CORAL_BUILD_TESTS)
  add_subdirectory(tests)
endif ()

# ==================================================================================================
# Examples
# ==================================================================================================
# The demo is a separate project that can be configured on its own (`cmake -S examples ...`); it is
# built as part of this repository only when this is the top-level project. An application that adds
# CoralSubtitle as a submodule gets the library it links and no example executables.
if (CORAL_SUBTITLE_TOP_LEVEL)
  add_subdirectory(examples)
endif ()
