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
        HitagiMath
        ${JPEG_LIBRARIES}
        png
        assimp::assimp
        spdlog::spdlog
)
target_include_directories(Parser
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        ${JPEG_INCLUDE_DIR}
)
