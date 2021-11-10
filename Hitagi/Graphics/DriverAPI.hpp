#pragma once
#include "Resource.hpp"
#include "Format.hpp"
#include "PipelineState.hpp"
#include "ICommandContext.hpp"

#include <vector>

namespace Hitagi::Graphics {
enum struct APIType {
    DirectX12,
    Vulkan,
    Melta,
    OpenGL
};

class DriverAPI {
public:
    virtual ~DriverAPI()                    = default;
    virtual void Present(size_t frameIndex) = 0;

    // Buffer
    virtual void           CreateSwapChain(uint32_t width, uint32_t height, unsigned frameCount, Format format, void* window)                     = 0;
    virtual RenderTarget   CreateRenderTarget(std::string_view name, const RenderTarget::Description& desc)                                       = 0;
    virtual RenderTarget   CreateRenderFromSwapChain(size_t frameIndex)                                                                           = 0;
    virtual VertexBuffer   CreateVertexBuffer(std::string_view name, size_t vertexCount, size_t vertexSize, const uint8_t* initialData = nullptr) = 0;
    virtual IndexBuffer    CreateIndexBuffer(std::string_view name, size_t indexCount, size_t indexSize, const uint8_t* initialData = nullptr)    = 0;
    virtual ConstantBuffer CreateConstantBuffer(std::string_view name, size_t numElements, size_t elementSize)                                    = 0;
    virtual TextureBuffer  CreateTextureBuffer(std::string_view name, const TextureBuffer::Description& desc)                                     = 0;
    virtual DepthBuffer    CreateDepthBuffer(std::string_view name, const DepthBuffer::Description& desc)                                         = 0;

    virtual Resource GetSwapChainBuffer(size_t frameIndex) = 0;

    virtual void UpdateConstantBuffer(ConstantBuffer& buffer, size_t offset, const uint8_t* src, size_t size) = 0;

    virtual void RetireResources(std::vector<Resource>&& resources, uint64_t fenceValue) = 0;
    // Sampler
    virtual Sampler CreateSampler(std::string_view name, const Graphics::Sampler::Description& desc) = 0;

    // Pipeline
    virtual std::unique_ptr<backend::Resource> CreateRootSignature(const RootSignature& rootsignature) = 0;
    virtual std::unique_ptr<backend::Resource> CreatePipelineState(const PipelineState& pso)           = 0;

    // CommandList
    virtual std::shared_ptr<IGraphicsCommandContext> GetGraphicsCommandContext() = 0;

    // Fence
    virtual void WaitFence(uint64_t fenceValue) = 0;
    virtual void IdleGPU()                      = 0;

    APIType GetType() const { return m_Type; }

    virtual void test(RenderTarget& rt, const PipelineState& pso) = 0;

protected:
    DriverAPI(APIType type) : m_Type(type) {}
    const APIType m_Type;
};

}  // namespace Hitagi::Graphics