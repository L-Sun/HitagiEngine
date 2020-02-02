add_library(Parser
    BMP.cpp
    FBX.cpp
    JPEG.cpp
    OGEX.cpp
    PNG.cpp
    TGA.cpp
)

target_link_libraries(Parser
    PRIVATE
        Interface
        CommonHeader
        GeomMath
        ${OPENGEX_LIBRARY} 
        ${OPENDDL_LIBRARY}
        ${JPEG_LIBRARIES}
        png
        ${FBX_LIBRARIES}
)
target_include_directories(Parser
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        ${FBX_INCLUDE_DIR}
        ${JPEG_INCLUDE_DIR}
)

