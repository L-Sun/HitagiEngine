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

    if(${APPLE})
        add_custom_command(OUTPUT ${FUNC}.o
            COMMAND ${CMAKE_COMMAND} -E env "PATH=${ISPC_COMPILER_PATH}" ${ISPC_COMPILER} ${ISPC_OPTIONS} -o ${FUNC}.o -h ${GEOMMATH_LIB_HEADER_FOLDER}/${FUNC}.h ${CMAKE_CURRENT_SOURCE_DIR}/${FUNC}.ispc
            COMMAND sed -i '' 's/const uint32_t count/const size_t count/' ${GEOMMATH_LIB_HEADER_FOLDER}/${FUNC}.h
            COMMAND sed -i '' 's/float\) [\*] f/\*f\)\(float\)/' ${GEOMMATH_LIB_HEADER_FOLDER}/${FUNC}.h # quick fix function pointer error
            MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/${FUNC}.ispc
            COMMENT "Building ${FUNC}"
        )
    elseif(${CMAKE_HOST_SYSTEM_NAME} MATCHES FreeBSD)
        add_custom_command(OUTPUT ${FUNC}.o
            COMMAND ${CMAKE_COMMAND} -E env "PATH=${ISPC_COMPILER_PATH}" ${ISPC_COMPILER} ${ISPC_OPTIONS} -o ${FUNC}.o -h ${GEOMMATH_LIB_HEADER_FOLDER}/${FUNC}.h ${CMAKE_CURRENT_SOURCE_DIR}/${FUNC}.ispc
            COMMAND gsed -i 's/const uint32_t count/const size_t count/' ${GEOMMATH_LIB_HEADER_FOLDER}/${FUNC}.h
            COMMAND gsed -i 's/float\) [\*] f/\*f\)\(float\)/' ${GEOMMATH_LIB_HEADER_FOLDER}/${FUNC}.h # quick fix function pointer error
            MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/${FUNC}.ispc
            COMMENT "Building ${FUNC}"
        )
    elseif(${CMAKE_HOST_SYSTEM_NAME} MATCHES Windows)
        add_custom_command(OUTPUT ${FUNC}.o
            COMMAND ${CMAKE_COMMAND} -E env "PATH=${ISPC_COMPILER_PATH}" ${ISPC_COMPILER} --woff ${ISPC_OPTIONS} -o ${FUNC}.o -h ${GEOMMATH_LIB_HEADER_FOLDER}/${FUNC}.h ${CMAKE_CURRENT_SOURCE_DIR}/${FUNC}.ispc

            MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/${FUNC}.ispc
            COMMENT "Building ${FUNC}"
        )
    else()
        add_custom_command(OUTPUT ${FUNC}.o
            COMMAND ${CMAKE_COMMAND} -E env "PATH=${ISPC_COMPILER_PATH}" ${ISPC_COMPILER} ${ISPC_OPTIONS} -o ${FUNC}.o -h ${GEOMMATH_LIB_HEADER_FOLDER}/${FUNC}.h ${CMAKE_CURRENT_SOURCE_DIR}/${FUNC}.ispc
            COMMAND sed -i 's/const uint32_t count/const size_t count/' ${GEOMMATH_LIB_HEADER_FOLDER}/${FUNC}.h
            COMMAND sed -i 's/float\) [\*] f/\*f\)\(float\)/' ${GEOMMATH_LIB_HEADER_FOLDER}/${FUNC}.h # quick fix function pointer error
            MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/${FUNC}.ispc
            COMMENT "Building ${FUNC}"
        )
    endif(${APPLE})

    list(APPEND OBJECTS ${FUNC}.o)

endforeach(FUNC)

add_custom_command(OUTPUT ${GEOMMATH_LIB_FILE}
        COMMAND ${CMAKE_AR} ${ISPC_LIBRARIAN_OPTIONS} ${OBJECTS}
        COMMAND rm -v ${OBJECTS}
        DEPENDS ${OBJECTS}
        )

add_custom_target(MathLib
                   DEPENDS ${GEOMMATH_LIB_FILE}
        )