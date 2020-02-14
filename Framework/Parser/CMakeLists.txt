add_library(Parser
    BMP.cpp
    Assimp.cpp
    JPEG.cpp
    PNG.cpp
    TGA.cpp
)

target_link_libraries(Parser
    PRIVATE
        Interface
        MemoryManager
        GeomMath
        ${JPEG_LIBRARIES}
        png
        assimp::assimp AssetLoader
)
target_include_directories(Parser
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        ${JPEG_INCLUDE_DIR}
        "$<TARGET_PROPERTY:CommonHeader,INTERFACE_INCLUDE_DIRECTORIES>"
)
target_compile_options(Parser PRIVATE -Wno-pragma-pack)

