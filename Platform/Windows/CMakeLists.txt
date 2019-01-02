add_library(WindowsApplication WindowsApplication.cpp)


find_package(OpenGL REQUIRED)

add_executable(MyGameEngineOpenGL
    OpenGLApplication.cpp
    ${PROJECT_SOURCE_DIR}/External/src/glad/src/glad_wgl.c
)

target_link_libraries(MyGameEngineOpenGL WindowsApplication OpenGLRHI Common
 ${OPENGEX_LIBRARY} ${OPENDDL_LIBRARY} ${XG_LIBRARY} ${OPENGL_LIBRARIES} ${ZLIB_LIBRARY})

target_include_directories(MyGameEngineOpenGL PRIVATE "${PROJECT_SOURCE_DIR}/External/src/glad/include")
__add_xg_platform_dependencies(MyGameEngineOpenGL)