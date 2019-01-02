add_executable(Empty EmptyApplication.cpp)
target_link_libraries(Empty Common ${XG_LIBRARY} ${OPENGEX_LIBRARY} ${OPENDDL_LIBRARY} ${ZLIB_LIBRARY}) 