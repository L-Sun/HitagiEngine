add_library(Parser INTERFACE)

target_link_libraries(Parser
    INTERFACE
        GeomMath
        ${OPENGEX_LIBRARY} 
        ${OPENDDL_LIBRARY}
        ${JPEG_LIBRARIES}
        png
)
target_include_directories(Parser INTERFACE ${CMAKE_CURRENT_SOURCE_DIR} ${JPEG_INCLUDE_DIR})