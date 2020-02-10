# Empty
add_library(Platform INTERFACE)

add_subdirectory(Empty)
IF(${WIN32})
    # Windows specific code
    add_subdirectory(Windows)
    target_include_directories(Platform INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/Windows)
ELSEIF(${UNIX})
    # Linux specific code
    add_subdirectory(Linux)
    target_include_directories(Platform INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/Linux)
ENDIF(${WIN32})