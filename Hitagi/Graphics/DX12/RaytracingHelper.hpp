#pragma once
#include "D3DCore.hpp"
#include "Buffer.hpp"
#include "RootSignature.hpp"
#include "HitagiMath.hpp"

namespace Hitagi::Graphics::DX12 {

class BottomLevelASGenerator {
public:
    void AddMesh(const MeshInfo& mesh, bool opaque = true);

    Microsoft::WRL::ComPtr<ID3D12Resource> Generate(CommandContext& context,
                                                    bool            updateOnly,
                                                    ID3D12Resource* previousResult);

    void Reset() {
        m_ScratchBuffer = GpuBuffer();
        m_Geometrydesc.clear();
        m_BuildDesc = {};
    }

private:
    GpuBuffer                                          m_ScratchBuffer;
    std::vector<D3D12_RAYTRACING_GEOMETRY_DESC>        m_Geometrydesc;
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC m_BuildDesc;
};

class TopLevelASGenerator {
public:
    void AddInstance(Microsoft::WRL::ComPtr<ID3D12Resource> bottomLevelAS,
                     const mat4f&                           transform,
                     int                                    instanceID,
                     int                                    hitGroupIndex);

    Microsoft::WRL::ComPtr<ID3D12Resource> Generate(CommandContext& context,
                                                    bool            updateOnly,
                                                    ID3D12Resource* previousResult);

    void Reset() {
        m_ScratchBuffer  = GpuBuffer();
        m_InstanceBuffer = GpuBuffer();
        m_InstanceDescs.clear();
        m_BuildDesc = {};
    }

private:
    GpuBuffer                                          m_ScratchBuffer;
    GpuBuffer                                          m_InstanceBuffer;
    std::vector<D3D12_RAYTRACING_INSTANCE_DESC>        m_InstanceDescs;
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC m_BuildDesc;
};

class RaytracingPipelineGenerator {
public:
    RaytracingPipelineGenerator() : m_GlobalRootSignature(nullptr) {
        m_DummyLocalRootSignature.Finalize(D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE);
        m_DummyGlobalRootSignature.Finalize();
    }
    void AddLibrary(const Core::Buffer& dxiLibrary, const std::vector<std::wstring>& symbolExports);

    void AddHitGroup(std::wstring_view hitGroupName,
                     std::wstring_view closestHitSymbol,
                     std::wstring_view anyHitSymbol       = L"",
                     std::wstring_view intersectionSymbol = L"");

    void AddRootSignatureAssociation(const RootSignature& rootSignature, const std::vector<std::wstring>& symbols);
    void SetGlobalSignature(const RootSignature& rootSignature);
    void SetMaxPayloadSize(unsigned size);

    void SetMaxAttributeSize(unsigned size);

    void SetMaxRecursionDepth(unsigned maxDepth);

    Microsoft::WRL::ComPtr<ID3D12StateObject> Generate();

private:
    struct Library {
        Library(const Core::Buffer& dxil, const std::vector<std::wstring>& exportedSymbols);

        const Core::Buffer&             m_Dxil;
        const std::vector<std::wstring> m_ExportedSymbols;

        std::vector<D3D12_EXPORT_DESC> m_Exports;
        D3D12_DXIL_LIBRARY_DESC        m_LibDesc;
    };

    struct HitGroup {
        HitGroup(std::wstring_view hitGroupName,
                 std::wstring_view closestHitSymbol,
                 std::wstring_view anyHitSymbol       = L"",
                 std::wstring_view intersectionSymbol = L"");

        std::wstring         m_HitGroupName;
        std::wstring         m_ClosestHitSymbol;
        std::wstring         m_AnyHitSymbol;
        std::wstring         m_IntersectionSymbol;
        D3D12_HIT_GROUP_DESC m_Desc;
    };

    struct RootSignatureAssociation {
        RootSignatureAssociation(const RootSignature& rootSignature, const std::vector<std::wstring>& symbols);

        ID3D12RootSignature*                   m_RootSignature;
        std::vector<std::wstring>              m_Symbols;
        std::vector<LPCWSTR>                   m_SymbolPointers;
        D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION m_Association;
    };

    void BuildShaderExportList(std::vector<std::wstring>& exportedSymbols);

    std::vector<Library>                  m_Libraries;
    std::vector<HitGroup>                 m_HitGroups;
    std::vector<RootSignatureAssociation> m_RootSignatureAssociations;
    unsigned                              m_MaxPayLoadSize = 0;
    // Attribute size, initialized to 2 for the barycentric coordinates used by the built-in triangle
    // intersection shader
    unsigned m_MaxAttributeSize  = 2 * sizeof(float);
    unsigned m_maxRecursionDepth = 1;

    RootSignature m_DummyLocalRootSignature;
    RootSignature m_DummyGlobalRootSignature;

    ID3D12RootSignature* m_GlobalRootSignature;
};

class ShaderTable {
public:
    ShaderTable(std::wstring_view name = L"") : m_Name(name), m_NumRecords(0), m_Stride(0) {
        // the max size of root signature is 256 bytes
        m_Records.reserve(D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + 256);
    }

    template <typename... Args>
    void AddShaderRecord(const void* shaderIdentifier, const Args&... args) {
        // C++17 flod experssion
        const size_t         recordMaxSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + (0 + ... + align(sizeof(Args), 8));
        std::vector<uint8_t> record(recordMaxSize);
        uint8_t*             p = record.data();
        // Process Identifier
        auto id = reinterpret_cast<const uint8_t*>(shaderIdentifier);
        std::copy(id, id + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, record.begin());
        p += D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

        // Process Args
        p = processArg(p, false, args...);

        // Update stride of record
        m_Stride = std::max(m_Stride, align(p - record.data(), D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT));

        m_Records.emplace_back(std::move(record));
        m_NumRecords++;
    }

    void Generate() {
        std::vector<uint8_t> table;
        table.reserve(m_NumRecords * m_Stride);
        for (auto&& record : m_Records) {
            table.insert(table.end(),
                         std::make_move_iterator(record.begin()),
                         std::make_move_iterator(record.end()));
            // add padding
            table.resize(table.size() + m_Stride - record.size());
        }

        m_Buffer.Create(m_Name, m_NumRecords, m_Stride, table.data());
        m_Records.clear();
    }

    GpuBuffer& GetGpuBuffer() {
        return m_Buffer;
    }
    const GpuBuffer& GetGpuBuffer() const {
        return m_Buffer;
    }
    D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() {
        return m_Buffer->GetGPUVirtualAddress();
    }
    size_t   GetSize() const { return m_NumRecords * m_Stride; }
    unsigned GetStride() const { return m_Stride; }

private:
    template <typename T, typename... Args>
    uint8_t* processArg(uint8_t* p, bool padding, const T& head, const Args&... args) {
        size_t alignedSize = align(sizeof(T), 4);
        if ((alignedSize & 7) != 0) padding = !padding;
        auto data = reinterpret_cast<const uint8_t*>(&head);
        std::copy(data, data + sizeof(T), p);
        p += alignedSize;
        return processArg(p, false, args...);
    }
    uint8_t* processArg(uint8_t* p, bool padding) {
        return p;
    }

    std::wstring                      m_Name;
    std::vector<std::vector<uint8_t>> m_Records;
    GpuBuffer                         m_Buffer;
    unsigned                          m_NumRecords;
    size_t                            m_Stride;
};

}  // namespace Hitagi::Graphics::DX12