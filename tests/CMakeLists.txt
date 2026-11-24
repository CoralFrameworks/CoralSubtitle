# ==================================================================================================
# CoralSubtitle tests
# ==================================================================================================
# One executable for the renderer and the layout helpers. It needs a font stack, which CoralGraphics
# provides on every host; a configure without the font support builds the library but cannot run the
# text tests, so they report themselves as skipped.
add_executable(test_Subtitle test_Subtitle.cpp)
target_link_libraries(test_Subtitle PRIVATE subtitle gtest)
set_target_properties(test_Subtitle PROPERTIES FOLDER Tests)

add_test(NAME test_Subtitle COMMAND test_Subtitle)
