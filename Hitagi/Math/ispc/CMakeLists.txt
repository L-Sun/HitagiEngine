find_program(ISPC_EXECUTABLE ispc)
if (NOT ISPC_EXECUTABLE)
    message(FATAL_ERROR "Failed to find ispc" )
endif(NOT ISPC_EXECUTABLE)

add_library(ispcMath)
set_target_properties(ispcMath PROPERTIES LINKER_LANGUAGE CXX)

set(ISPC_SRC "vector")
set(ISPC_FLAGS -O2)

foreach(ISPC_SRC_NAME IN LISTS ISPC_SRC)
    set(ISPC_HEADER_NAME "${CMAKE_CURRENT_SOURCE_DIR}/${ISPC_SRC_NAME}_ispc.h")
    set(ISPC_OBJ_NAME "${CMAKE_CURRENT_BINARY_DIR}/${ISPC_SRC_NAME}_ispc${CMAKE_CXX_OUTPUT_EXTENSION}")
    if(UNIX)
        list(APPEND ISPC_FLAGS --pic)
    endif(UNIX)

    add_custom_command(
        OUTPUT
            ${ISPC_HEADER_NAME}
            ${ISPC_OBJ_NAME}
        COMMAND 
            ${ISPC_EXECUTABLE} 
            ${CMAKE_CURRENT_SOURCE_DIR}/${ISPC_SRC_NAME}.ispc 
            ${ISPC_FLAGS}
            --target=host
            -h ${ISPC_HEADER_NAME}
            -o ${ISPC_OBJ_NAME}
        VERBATIM
        DEPENDS ${ISPC_EXECUTABLE}
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${ISPC_SRC_NAME}.ispc")
    
    if(WIN32)
        set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/${ISPC_SRC_NAME}.ispc" PROPERTIES HEADER_FILE_ONLY TRUE)
    endif(WIN32)
    target_sources(ispcMath PRIVATE ${ISPC_OBJ_NAME})
endforeach()
target_include_directories(ispcMath INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
