find_package(ZLIB REQUIRED)

add_library(Image Image.cpp)
add_library(Parser INTERFACE)
target_include_directories(Parser INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(Parser INTERFACE
    Image
    GeomMath
    ${TURBO_JPEG_LIBRARY}
    ${OPENGEX_LIBRARY} 
    ${OPENDDL_LIBRARY}
    ${XG_LIBRARY}
    ZLIB::ZLIB
)