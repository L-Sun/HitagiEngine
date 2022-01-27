add_library(MANN MANN.cpp)
target_link_libraries(MANN
    PUBLIC
        ort::onnxruntime
    PRIVATE
        ort::providers
)
target_include_directories(MANN
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

