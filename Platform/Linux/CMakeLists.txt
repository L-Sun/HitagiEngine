
add_library(GLFWApplication GLFWApplication.cpp)

find_library(OPENGL_LIBRARY GL)

if(${CMAKE_HOST_SYSTEM_NAME} MATCHES FreeBSD)
set (DL_LIBRARY "")
else(${CMAKE_HOST_SYSTEM_NAME} MATCHES FreeBSD)
find_library(DL_LIBRARY dl)
endif(${CMAKE_HOST_SYSTEM_NAME} MATCHES FreeBSD)


add_executable(MyGameEngineOpenGL OpenGLApplication.cpp)

target_link_libraries(MyGameEngineOpenGL GLFWApplication OpenGLRHI Common glfw ${OPENGEX_LIBRARY} ${OPENDDL_LIBRARY} ${XG_LIBRARY} ${OPENGL_LIBRARY} ${DL_LIBRARY})

target_include_directories(MyGameEngineOpenGL PRIVATE "${PROJECT_SOURCE_DIR}/External/src/glad/include")
__add_xg_platform_dependencies(MyGameEngineOpenGL)