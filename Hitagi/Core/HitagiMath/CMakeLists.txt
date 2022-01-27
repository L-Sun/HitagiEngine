if(USE_ISPC)
    add_subdirectory(ispc)
endif(USE_ISPC)

add_library(HitagiMath INTERFACE)
target_link_libraries(HitagiMath INTERFACE fmt::fmt $<$<BOOL:${USE_ISPC}>:ispcMath>)
target_compile_definitions(HitagiMath INTERFACE $<$<BOOL:${USE_ISPC}>:USE_ISPC>)
target_include_directories(HitagiMath INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
