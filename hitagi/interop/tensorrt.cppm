module;
#include <NvInfer.h>
#include <cuda_runtime_api.h>

export module interop.tensorrt;
export namespace hitagi::interop::trt {
using ::nvinfer1::DataType;
using ::nvinfer1::ICudaEngine;
using ::nvinfer1::IExecutionContext;
using ::nvinfer1::ILogger;
using ::nvinfer1::IRuntime;
inline auto createInferRuntime(ILogger& logger) noexcept -> IRuntime* {
    return ::nvinfer1::createInferRuntime(logger);
}
}  // namespace hitagi::interop::trt
export namespace hitagi::interop {
using ::cudaError_t;
using ::cudaFree;
using ::cudaGetErrorString;
using ::cudaMalloc;
using ::cudaMemcpyAsync;
using ::cudaMemcpyDeviceToHost;
using ::cudaMemcpyHostToDevice;
using ::cudaStream_t;
using ::cudaStreamCreate;
using ::cudaStreamDestroy;
using ::cudaStreamSynchronize;
using ::cudaSuccess;
}  // namespace hitagi::interop
