#pragma once
#include "../ICommandContext.hpp"

#include "HitagiMath.hpp"
#include "CommandListManager.hpp"
#include "GpuResource.hpp"
#include "Allocator.hpp"
#include "DynamicDescriptorHeap.hpp"
#include "PSO.hpp"

namespace Hitagi::Graphics::backend::DX12 {
class DX12DriverAPI;

class CommandContext {
public:
    CommandContext(DX12DriverAPI& driver, D3D12_COMMAND_LIST_TYPE type);
    CommandContext(const CommandContext&) = delete;
    CommandContext& operator=(const CommandContext&) = delete;
    CommandContext(CommandContext&&)                 = default;
    CommandContext& operator=(CommandContext&&) = delete;
    ~CommandContext();

    auto GetCommandList() noexcept { return m_CommandList; }

    void TransitionResource(GpuResource& resource, D3D12_RESOURCE_STATES new_state, bool flush_immediate = false);
    void FlushResourceBarriers();

    void SetPSO(const PSO& pso) { m_CommandList->SetPipelineState(pso.GetPSO()); }

    void SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, ID3D12DescriptorHeap* heap) {
        if (m_CurrentDescriptorHeaps[type] != heap) {
            m_CurrentDescriptorHeaps[type] = heap;
            BindDescriptorHeaps();
        }
    }

    void SetDynamicDescriptor(unsigned root_index, unsigned offset, const Descriptor& descriptor) {
        SetDynamicDescriptors(root_index, offset, {descriptor});
    }
    void SetDynamicDescriptors(unsigned root_index, unsigned offset, const std::vector<Descriptor>& descriptors) {
        m_DynamicViewDescriptorHeap.StageDescriptors(root_index, offset, descriptors);
    }
    void SetDynamicSampler(unsigned root_index, unsigned offset, const Descriptor& descriptor) {
        SetDynamicSamplers(root_index, offset, {descriptor});
    }
    void SetDynamicSamplers(unsigned root_index, unsigned offset, const std::vector<Descriptor>& descriptors) {
        m_DynamicSamplerDescriptorHeap.StageDescriptors(root_index, offset, descriptors);
    }
    uint64_t Finish(bool wait_for_complete = false);
    void     Reset();

protected:
    void BindDescriptorHeaps();

    DX12DriverAPI&              m_Driver;
    ID3D12GraphicsCommandList5* m_CommandList      = nullptr;
    ID3D12CommandAllocator*     m_CommandAllocator = nullptr;

    std::array<D3D12_RESOURCE_BARRIER, 16> m_Barriers{};
    unsigned                               m_NumBarriersToFlush = 0;

    LinearAllocator m_CpuLinearAllocator;
    LinearAllocator m_GpuLinearAllocator;

    std::array<ID3D12DescriptorHeap*, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> m_CurrentDescriptorHeaps{};

    DynamicDescriptorHeap m_DynamicViewDescriptorHeap;     // HEAP_TYPE_CBV_SRV_UAV
    DynamicDescriptorHeap m_DynamicSamplerDescriptorHeap;  // HEAP_TYPE_SAMPLER

    ID3D12RootSignature* m_RootSignature{};

    D3D12_COMMAND_LIST_TYPE m_Type;
};

class GraphicsCommandContext : public CommandContext, public Graphics::IGraphicsCommandContext {
public:
    GraphicsCommandContext(DX12DriverAPI& driver)
        : CommandContext(driver, D3D12_COMMAND_LIST_TYPE_DIRECT) {}

    // Front end interface
    void SetViewPort(uint32_t x, uint32_t y, uint32_t width, uint32_t height) final;
    void SetScissorRect(uint32_t left, uint32_t top, uint32_t right, uint32_t bottom) final;
    void SetViewPortAndScissor(uint32_t x, uint32_t y, uint32_t width, uint32_t height) final;
    void SetRenderTarget(Graphics::RenderTarget& rt) final;
    void UnsetRenderTarget() final;
    void SetRenderTargetAndDepthBuffer(Graphics::RenderTarget& rt, Graphics::DepthBuffer& depth_buffer) final;
    void ClearRenderTarget(Graphics::RenderTarget& rt) final;
    void ClearDepthBuffer(Graphics::DepthBuffer& depth_buffer) final;
    void SetPipelineState(const Graphics::PipelineState& pipeline) final;
    void SetParameter(std::string_view name, const Graphics::ConstantBuffer& cb, size_t offset) final;
    void SetParameter(std::string_view name, const Graphics::TextureBuffer& texture) final;
    void SetParameter(std::string_view name, const Graphics::Sampler& sampler) final;

    void UpdateBuffer(std::shared_ptr<Graphics::Resource> resource, size_t offset, const uint8_t* data, size_t data_size) final;

    void     Draw(const Graphics::MeshBuffer& mesh) final;
    void     Present(Graphics::RenderTarget& rt) final;
    uint64_t Finish(bool wait_for_complete = false) final { return CommandContext::Finish(wait_for_complete); }

    // Back end interface
    void SetRootSignature(const RootSignature& root_signature) {
        if (m_RootSignature == root_signature.GetRootSignature()) return;

        m_CommandList->SetGraphicsRootSignature(m_RootSignature = root_signature.GetRootSignature());
        m_DynamicViewDescriptorHeap.ParseRootSignature(root_signature);
        m_DynamicSamplerDescriptorHeap.ParseRootSignature(root_signature);
    }

private:
    const Graphics::PipelineState* m_Pipeline{};
};

class ComputeCommandContext : public CommandContext {
public:
    ComputeCommandContext(DX12DriverAPI& driver)
        : CommandContext(driver, D3D12_COMMAND_LIST_TYPE_COMPUTE) {}
};

class CopyCommandContext : public CommandContext {
public:
    CopyCommandContext(DX12DriverAPI& driver)
        : CommandContext(driver, D3D12_COMMAND_LIST_TYPE_COPY) {}

    void InitializeBuffer(GpuResource& dest, const uint8_t* data, size_t data_size);
    void InitializeTexture(GpuResource& dest, const std::vector<D3D12_SUBRESOURCE_DATA>& sub_data);
};

}  // namespace Hitagi::Graphics::backend::DX12