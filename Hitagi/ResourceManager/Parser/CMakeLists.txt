add_library(Parser
    BMP.cpp
    Assimp.cpp
    JPEG.cpp
    PNG.cpp
    TGA.cpp
)

target_link_libraries(Parser
    PRIVATE
        MemoryManager
        GeomMath
        ${JPEG_LIBRARIES}
        png
        assimp::assimp
        spdlog::spdlog
)
target_include_directories(Parser
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        ${JPEG_INCLUDE_DIR}
)
target_compile_options(Parser PRIVATE -Wno-pragma-pack)