if(USE_ISPC)
    add_subdirectory(ispc)
endif(USE_ISPC)

add_library(Math INTERFACE)
target_link_libraries(Math INTERFACE fmt::fmt $<$<BOOL:${USE_ISPC}>:ispcMath>)
target_compile_definitions(Math INTERFACE $<$<BOOL:${USE_ISPC}>:USE_ISPC>)
target_include_directories(Math INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
