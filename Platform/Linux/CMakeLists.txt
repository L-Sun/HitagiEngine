
add_library(GLFWApplication GLFWApplication.cpp)

find_library(OPENGL_LIBRARY GL)

if(${CMAKE_HOST_SYSTEM_NAME} MATCHES FreeBSD)
set (DL_LIBRARY "")
else(${CMAKE_HOST_SYSTEM_NAME} MATCHES FreeBSD)
find_library(DL_LIBRARY dl)
endif(${CMAKE_HOST_SYSTEM_NAME} MATCHES FreeBSD)


add_library(HitagiGameEngine OpenGLApplication.cpp OpenGLApplicationConfig.cpp)

target_link_libraries(HitagiGameEngine GLFWApplication OpenGLRHI Common glfw
    ${OPENGL_LIBRARY}
    ${DL_LIBRARY}
)
