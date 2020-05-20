add_library(HitagiMath INTERFACE)
target_link_libraries(HitagiMath INTERFACE fmt::fmt)
target_include_directories(HitagiMath INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})