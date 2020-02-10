add_library(Parser
    BMP.cpp
    # FBX.cpp
    Assimp.cpp
    JPEG.cpp
    PNG.cpp
    TGA.cpp
)

target_link_libraries(Parser
    PRIVATE
        Interface
        CommonHeader
        MemoryManager
        GeomMath
        ${JPEG_LIBRARIES}
        png
        assimp::assimp AssetLoader
        # ${FBX_LIBRARIES}
)
target_include_directories(Parser
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        # ${FBX_INCLUDE_DIR}
        ${JPEG_INCLUDE_DIR}
)
target_compile_options(Parser PRIVATE -Wno-pragma-pack)

