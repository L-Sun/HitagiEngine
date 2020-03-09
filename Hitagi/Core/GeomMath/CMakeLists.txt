if(${CMAKE_HOST_SYSTEM_NAME} MATCHES "Windows")
    set(ISPC_COMPILER ispc.exe)
    if(NOT ${CMAKE_CROSSCOMPILING})
        set(ISPC_OPTIONS --arch=x86-64 --target=host)
        set(CMAKE_AR lib)
        set(ISPC_LIBRARIAN_OPTIONS /MACHINE:X64 /OUT:${GEOMMATH_LIB_FILE})
    ELSE()
        set(ISPC_OPTIONS --arch=arm --cpu=cortex-a9 --target=neon-i32x4)
        set(CMAKE_AR ${SNC_TOOLCHAIN_DIR}psp2snarl -rcv ${GEOMMATH_LIB_FILE})
    endif()
elseif(${CMAKE_HOST_SYSTEM_NAME} MATCHES "Linux")
    set(ISPC_COMPILER ispc)
    set(ISPC_OPTIONS --pic --arch=x86-64)
    set(ISPC_LIBRARIAN_OPTIONS -rcv -o ${GEOMMATH_LIB_FILE})
endif()
set(ISPC_OPTIONS ${ISPC_OPTIONS} -O2 --woff)

set(FUNCTIONS 
Addition
Subtraction
Multiplication
Division
Vector
Matrix
DCT
Absolute
MaxMin
)

foreach(FUNC IN LISTS FUNCTIONS)
    IF(APPLE)
        add_custom_command(OUTPUT ${FUNC}.o
            COMMAND ${CMAKE_COMMAND} -E env  ${ISPC_COMPILER} ${ISPC_OPTIONS} -o ${FUNC}.o ${CMAKE_CURRENT_SOURCE_DIR}/${FUNC}.ispc
            MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/${FUNC}.ispc
            COMMENT "Building ${FUNC}"
            )
    ELSEIF(${CMAKE_HOST_SYSTEM_NAME} MATCHES FreeBSD)
        add_custom_command(OUTPUT ${FUNC}.o
            COMMAND ${CMAKE_COMMAND} -E env  ${ISPC_COMPILER} ${ISPC_OPTIONS} -o ${FUNC}.o ${CMAKE_CURRENT_SOURCE_DIR}/${FUNC}.ispc
            MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/${FUNC}.ispc
            COMMENT "Building ${FUNC}"
            )
    ELSEIF(${CMAKE_HOST_SYSTEM_NAME} MATCHES Windows)
        add_custom_command(OUTPUT ${FUNC}.o
       COMMAND ${CMAKE_COMMAND} -E env  ${ISPC_COMPILER} ${ISPC_OPTIONS} -o ${FUNC}.o ${CMAKE_CURRENT_SOURCE_DIR}/${FUNC}.ispc
            MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/${FUNC}.ispc
            COMMENT "Building ${FUNC}"
            )
    ELSEIF(${CMAKE_SYSTEM_NAME} MATCHES Android)
        add_custom_command(OUTPUT ${FUNC}.o
            COMMAND ${CMAKE_COMMAND} -E env  ${ISPC_COMPILER} ${ISPC_OPTIONS} --emit-llvm -o ${FUNC}.bc ${CMAKE_CURRENT_SOURCE_DIR}/${FUNC}.ispc
            COMMAND ${CMAKE_COMMAND} -E env "PATH=${LLVM_COMPILER_PATH}" llc -O3 -float-abi=soft -march=arm -mcpu=cortex-a9 -thread-model=posix -filetype=obj -o=${FUNC}.o ${FUNC}.bc
            MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/${FUNC}.ispc
            COMMENT "Building ${FUNC}"
            )
    ELSE()
        add_custom_command(OUTPUT ${FUNC}.o
            COMMAND ${CMAKE_COMMAND} -E env  ${ISPC_COMPILER} ${ISPC_OPTIONS} -o ${FUNC}.o ${CMAKE_CURRENT_SOURCE_DIR}/${FUNC}.ispc
            MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/${FUNC}.ispc
            COMMENT "Building ${FUNC}"
            )
    ENDIF()

    list(APPEND OBJECTS ${FUNC}.o)

endforeach(FUNC)

IF(${WIN32})
add_custom_command(OUTPUT ${GEOMMATH_LIB_FILE}
        COMMAND ${CMAKE_AR} ${ISPC_LIBRARIAN_OPTIONS} ${OBJECTS}
        DEPENDS ${OBJECTS}
        )
ELSE(${WIN32})
add_custom_command(OUTPUT ${GEOMMATH_LIB_FILE}
        COMMAND ${CMAKE_AR} ${ISPC_LIBRARIAN_OPTIONS} ${OBJECTS}
        COMMAND ${CMAKE_RANLIB} ${GEOMMATH_LIB_FILE}
        DEPENDS ${OBJECTS}
        )
ENDIF(${WIN32})

add_custom_target(ISPCMathLib
                   DEPENDS ${GEOMMATH_LIB_FILE}
        )