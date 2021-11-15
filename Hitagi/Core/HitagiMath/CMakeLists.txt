if(USE_ISPC)
    add_subdirectory(ispc)
endif(USE_ISPC)

add_library(HitagiMath Geometry.cpp)
target_link_libraries(HitagiMath PUBLIC fmt::fmt $<$<BOOL:${USE_ISPC}>:ispcMath>)
target_compile_definitions(HitagiMath PUBLIC $<$<BOOL:${USE_ISPC}>:USE_ISPC>)
target_include_directories(HitagiMath PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})