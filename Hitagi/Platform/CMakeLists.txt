add_library(BaseApplication main.cpp BaseApplication.cpp)
target_link_libraries(BaseApplication PUBLIC DebugManager GameLogic Timer)
target_include_directories(BaseApplication INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(Empty)
IF(${WIN32})
    # Windows specific code
    add_subdirectory(Windows)
ELSEIF(${UNIX})
    # Linux specific code
    add_subdirectory(Linux)
ENDIF(${WIN32})