# ===============================================================
# Scene Test
# ===============================================================
add_executable(
    scene_loading_test
    scene_loading_test.cpp
)
target_link_libraries(
    scene_loading_test
    PRIVATE asset_manager
)
add_test(
    NAME TEST_scene_loading
    COMMAND scene_loading_test
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)

# ===============================================================
# Parser Test
# ===============================================================
add_executable(
    image_parser_test
    image_parser_test.cpp
)
target_link_libraries(
    image_parser_test
    PRIVATE asset_manager GTest::gtest
)
add_test(
    NAME TEST_image_parser
    COMMAND image_parser_test
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)

# ===============================================================
# Font Bitmap Test
# ===============================================================
add_executable(
    text_bitmap_test
    text_bitmap_test.cpp
)
target_link_libraries(
    text_bitmap_test
    PRIVATE freetype file_io_manager
)
