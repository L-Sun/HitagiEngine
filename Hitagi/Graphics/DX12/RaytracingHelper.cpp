#include "RaytracingHelper.hpp"
#include "CommandContext.hpp"

namespace Hitagi::Graphics::backend::DX12 {

void BottomLevelASGenerator::AddMesh(const MeshInfo& mesh, bool opaque) {
    auto vbv = mesh.verticesBuffer[0].VertexBufferView();
    auto ibv = mesh.indicesBuffer.IndexBufferView();

    D3D12_RAYTRACING_GEOMETRY_DESC desc;
    desc.Type  = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
    desc.Flags = opaque ? D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE : D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;

    D3D12_RAYTRACING_GEOMETRY_TRIANGLES_DESC& trianglesDesc = desc.Triangles;
    trianglesDesc.VertexFormat                              = DXGI_FORMAT_R32G32B32_FLOAT;
    trianglesDesc.VertexCount                               = vbv.SizeInBytes / vbv.StrideInBytes;
    trianglesDesc.VertexBuffer.StartAddress                 = vbv.BufferLocation;
    trianglesDesc.VertexBuffer.StrideInBytes                = vbv.StrideInBytes;
    trianglesDesc.IndexBuffer                               = ibv.BufferLocation;
    trianglesDesc.IndexCount                                = mesh.indexCount;
    trianglesDesc.IndexFormat                               = ibv.Format;
    trianglesDesc.Transform3x4                              = 0;

    m_Geometrydesc.push_back(std::move(desc));
}

Microsoft::WRL::ComPtr<ID3D12Resource> BottomLevelASGenerator::Generate(GraphicsCommandContext& context, bool updateOnly,
                                                                        ID3D12Resource* previousResult) {
    // Calculate buffer size
    auto& bottomLevelInputs          = m_BuildDesc.Inputs;
    bottomLevelInputs.Type           = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    bottomLevelInputs.NumDescs       = m_Geometrydesc.size();
    bottomLevelInputs.pGeometryDescs = m_Geometrydesc.data();
    bottomLevelInputs.Flags          = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
    bottomLevelInputs.DescsLayout    = D3D12_ELEMENTS_LAYOUT_ARRAY;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bottomLevelprebuildInfo;
    g_Device->GetRaytracingAccelerationStructurePrebuildInfo(&bottomLevelInputs, &bottomLevelprebuildInfo);

    auto scratchBufferSize = bottomLevelprebuildInfo.ScratchDataSizeInBytes;
    auto resultSize        = bottomLevelprebuildInfo.ResultDataMaxSizeInBytes;

    // Create Buffer
    m_ScratchBuffer.Create(L"BLAS Scratch Buffer", scratchBufferSize, 1);

    D3D12_HEAP_PROPERTIES                  defaultHeapDesc = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto                                   desc            = CD3DX12_RESOURCE_DESC::Buffer(resultSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    Microsoft::WRL::ComPtr<ID3D12Resource> resultBuffer;
    g_Device->CreateCommittedResource(&defaultHeapDesc, D3D12_HEAP_FLAG_NONE,
                                      &desc,
                                      D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
                                      nullptr,
                                      IID_PPV_ARGS(&resultBuffer));

    m_BuildDesc.DestAccelerationStructureData    = {resultBuffer->GetGPUVirtualAddress()};
    m_BuildDesc.ScratchAccelerationStructureData = {m_ScratchBuffer->GetGPUVirtualAddress()};
    m_BuildDesc.SourceAccelerationStructureData  = previousResult ? previousResult->GetGPUVirtualAddress() : 0;

    context.BuildRaytracingAccelerationStructure(resultBuffer.Get(), m_BuildDesc);

    return resultBuffer;
}

void TopLevelASGenerator::AddInstance(Microsoft::WRL::ComPtr<ID3D12Resource> bottomLevelAS, const mat4f& transform,
                                      int instanceID, int hitGroupIndex) {
    D3D12_RAYTRACING_INSTANCE_DESC desc;
    desc.AccelerationStructure               = bottomLevelAS->GetGPUVirtualAddress();
    desc.Flags                               = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
    desc.InstanceContributionToHitGroupIndex = hitGroupIndex;
    desc.InstanceID                          = instanceID;
    desc.InstanceMask                        = 0xff;

    ZeroMemory(desc.Transform, sizeof(desc.Transform));
    for (int row = 0; row < 3; row++)
        for (int col = 0; col < 4; col++) desc.Transform[row][col] = transform[row][col];

    m_InstanceDescs.push_back(std::move(desc));
}

Microsoft::WRL::ComPtr<ID3D12Resource> TopLevelASGenerator::Generate(GraphicsCommandContext& context, bool updateOnly,
                                                                     ID3D12Resource* previousResult) {
    // Calculate buffer size
    auto& topLevelInputs          = m_BuildDesc.Inputs;
    topLevelInputs.Type           = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    topLevelInputs.NumDescs       = m_InstanceDescs.size();
    topLevelInputs.Flags          = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
    topLevelInputs.pGeometryDescs = nullptr;
    topLevelInputs.DescsLayout    = D3D12_ELEMENTS_LAYOUT_ARRAY;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO topLevelPrebuildInfo;
    g_Device->GetRaytracingAccelerationStructurePrebuildInfo(&topLevelInputs, &topLevelPrebuildInfo);

    auto scratchBufferSize = topLevelPrebuildInfo.ScratchDataSizeInBytes;
    auto resultSize        = topLevelPrebuildInfo.ResultDataMaxSizeInBytes;

    // Create Buffer
    m_ScratchBuffer.Create(L"TLAS Scratch Buffer", scratchBufferSize, 1);

    D3D12_HEAP_PROPERTIES                  defaultHeapDesc = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto                                   desc            = CD3DX12_RESOURCE_DESC::Buffer(resultSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    Microsoft::WRL::ComPtr<ID3D12Resource> resultBuffer;
    g_Device->CreateCommittedResource(&defaultHeapDesc,
                                      D3D12_HEAP_FLAG_NONE,
                                      &desc,
                                      D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, nullptr,
                                      IID_PPV_ARGS(&resultBuffer));
    m_InstanceBuffer.Create(L"Instance Data Buffer",
                            m_InstanceDescs.size(),
                            sizeof(D3D12_RAYTRACING_INSTANCE_DESC),
                            m_InstanceDescs.data());

    topLevelInputs.InstanceDescs = m_InstanceBuffer->GetGPUVirtualAddress();

    m_BuildDesc.DestAccelerationStructureData    = {resultBuffer->GetGPUVirtualAddress()};
    m_BuildDesc.ScratchAccelerationStructureData = {m_ScratchBuffer->GetGPUVirtualAddress()};
    m_BuildDesc.SourceAccelerationStructureData  = updateOnly ? previousResult->GetGPUVirtualAddress() : 0;

    context.BuildRaytracingAccelerationStructure(resultBuffer.Get(), m_BuildDesc);
    return resultBuffer;
}

void RaytracingPipelineGenerator::AddLibrary(const Core::Buffer& dxiLibrary, const std::vector<std::wstring>& symbolExports) {
    m_Libraries.emplace_back(dxiLibrary, symbolExports);
}

void RaytracingPipelineGenerator::AddHitGroup(std::wstring_view hitGroupName,
                                              std::wstring_view closestHitSymbol,
                                              std::wstring_view anyHitSymbol,
                                              std::wstring_view intersectionSymbol) {
    m_HitGroups.emplace_back(hitGroupName, closestHitSymbol, anyHitSymbol, intersectionSymbol);
}

void RaytracingPipelineGenerator::AddRootSignatureAssociation(const RootSignature& rootSignature, const std::vector<std::wstring>& symbols) {
    m_RootSignatureAssociations.emplace_back(rootSignature, symbols);
}
void RaytracingPipelineGenerator::SetGlobalSignature(const RootSignature& rootSignature) {
    m_GlobalRootSignature = rootSignature.GetRootSignature();
    assert(m_GlobalRootSignature != nullptr);
}

void RaytracingPipelineGenerator::SetMaxPayloadSize(UINT size) {
    m_MaxPayLoadSize = size;
}

void RaytracingPipelineGenerator::SetMaxAttributeSize(UINT size) {
    m_MaxAttributeSize = size;
}

void RaytracingPipelineGenerator::SetMaxRecursionDepth(UINT maxDepth) {
    m_maxRecursionDepth = maxDepth;
}

Microsoft::WRL::ComPtr<ID3D12StateObject> RaytracingPipelineGenerator::Generate() {
    // The pipeline is made of a set of sub-objects
    size_t subobjectCount =
        m_Libraries.size() +                      // DXIL Libraries
        m_HitGroups.size() +                      // Hit group declarations
        1 +                                       // Shader configuration
        1 +                                       // Shader payload
        2 * m_RootSignatureAssociations.size() +  // Root signature declartion + association
        2 +                                       // empty global and local root signatures
        1;                                        // final pipeline subobject

    std::vector<D3D12_STATE_SUBOBJECT> subobjects;
    subobjects.reserve(subobjectCount);

    // Add all the DXIL libraries
    for (const auto& lib : m_Libraries) {
        D3D12_STATE_SUBOBJECT libSubobject;
        libSubobject.Type  = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
        libSubobject.pDesc = &lib.m_LibDesc;
        subobjects.emplace_back(std::move(libSubobject));
    }

    // Add all the hit group declarations
    for (const auto& group : m_HitGroups) {
        D3D12_STATE_SUBOBJECT hitGroup;
        hitGroup.Type  = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
        hitGroup.pDesc = &group.m_Desc;
        subobjects.emplace_back(std::move(hitGroup));
    }

    // Add a subobject for the shader payload configuration
    D3D12_RAYTRACING_SHADER_CONFIG shaderDesc;
    shaderDesc.MaxPayloadSizeInBytes   = m_MaxPayLoadSize;
    shaderDesc.MaxAttributeSizeInBytes = m_MaxAttributeSize;

    D3D12_STATE_SUBOBJECT shaderConfigObject;
    shaderConfigObject.Type  = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
    shaderConfigObject.pDesc = &shaderDesc;
    subobjects.emplace_back(std::move(shaderConfigObject));

    // Build a list of all the symbols for ray generation, miss and hit groups
    // Those shaders have to be associated with the payload definition
    std::vector<std::wstring> exportedSymbols;
    std::vector<LPCWSTR>      exportedSymbolPointers;
    BuildShaderExportList(exportedSymbols);

    // Build an array of the string pointers
    exportedSymbolPointers.reserve(exportedSymbols.size());
    for (const auto& name : exportedSymbols)
        exportedSymbolPointers.emplace_back(name.c_str());

    // Add a subobject for the association between shaders and the payload
    D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION shaderPayloadAssociation;
    shaderPayloadAssociation.NumExports = exportedSymbols.size();
    shaderPayloadAssociation.pExports   = exportedSymbolPointers.data();

    // Associate the set of shaders with the payload defined in the previous subobject
    shaderPayloadAssociation.pSubobjectToAssociate = &subobjects.back();

    // Create and store the payload association object
    D3D12_STATE_SUBOBJECT shaderPayloadAssociationObject;
    shaderPayloadAssociationObject.Type  = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
    shaderPayloadAssociationObject.pDesc = &shaderPayloadAssociation;
    subobjects.emplace_back(std::move(shaderPayloadAssociationObject));

    // The root signature association requires two objects for each: one to declare the root
    // signature, and another to associate that root signature to a set of symbols
    for (auto& assoc : m_RootSignatureAssociations) {
        // Add a subobject to declare the root signature
        D3D12_STATE_SUBOBJECT rootSigObject;
        rootSigObject.Type  = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
        rootSigObject.pDesc = &assoc.m_RootSignature;
        subobjects.emplace_back(std::move(rootSigObject));

        // Add a subobject for the association between the exported shader symbols and the root
        // signature
        assoc.m_Association.NumExports            = assoc.m_SymbolPointers.size();
        assoc.m_Association.pExports              = assoc.m_SymbolPointers.data();
        assoc.m_Association.pSubobjectToAssociate = &subobjects.back();

        D3D12_STATE_SUBOBJECT rootSigAssociationObject;
        rootSigAssociationObject.Type  = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
        rootSigAssociationObject.pDesc = &assoc.m_Association;
        subobjects.emplace_back(std::move(rootSigAssociationObject));
    }

    // The pipeline construction always requires an empty global root signature
    D3D12_STATE_SUBOBJECT globalRootSig;
    globalRootSig.Type  = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
    auto gsig           = m_DummyGlobalRootSignature.GetRootSignature();
    globalRootSig.pDesc = m_GlobalRootSignature ? &m_GlobalRootSignature : &gsig;
    subobjects.emplace_back(std::move(globalRootSig));

    // Add a subobject for the ray tracing pipeline configuration
    D3D12_RAYTRACING_PIPELINE_CONFIG pipelineConfig;
    pipelineConfig.MaxTraceRecursionDepth = m_maxRecursionDepth;

    D3D12_STATE_SUBOBJECT pipelineConfigObject;
    pipelineConfigObject.Type  = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
    pipelineConfigObject.pDesc = &pipelineConfig;
    subobjects.emplace_back(std::move(pipelineConfigObject));

    // Describe the ray tracing pipeline state object
    D3D12_STATE_OBJECT_DESC pipelineDesc;
    pipelineDesc.Type          = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
    pipelineDesc.NumSubobjects = subobjects.size();
    pipelineDesc.pSubobjects   = subobjects.data();

    Microsoft::WRL::ComPtr<ID3D12StateObject> rtStateObject;

    // Create the state object
    HRESULT hr = g_Device->CreateStateObject(&pipelineDesc, IID_PPV_ARGS(&rtStateObject));
    if (FAILED(hr)) {
        throw std::logic_error("Could not create the raytracing state object");
    }
    return rtStateObject;
}

void RaytracingPipelineGenerator::BuildShaderExportList(std::vector<std::wstring>& exportedSymbols) {
    std::unordered_set<std::wstring> exports;

    for (const auto& lib : m_Libraries) {
        for (const auto& exportName : lib.m_ExportedSymbols) {
#ifdef _DEBUG
            // Sanity check in debug mode: check that no name is exported more than once
            if (exports.find(exportName) != exports.end()) {
                throw std::logic_error("Multiple definition of a symbol in the imported DXIL libraries");
            }
#endif
            exports.insert(exportName);
        }
    }
#ifdef _DEBUG
    // Sanity check in debug mode: verify that the hit groups do not reference an unknown shader name
    std::unordered_set<std::wstring> all_exports = exports;

    for (const auto& hitGroup : m_HitGroups) {
        if (!hitGroup.m_AnyHitSymbol.empty() &&
            exports.find(hitGroup.m_AnyHitSymbol) == exports.end()) {
            throw std::logic_error("Any hit symbol not found in the imported DXIL libraries");
        }

        if (!hitGroup.m_ClosestHitSymbol.empty() &&
            exports.find(hitGroup.m_ClosestHitSymbol) == exports.end()) {
            throw std::logic_error("Closest hit symbol not found in the imported DXIL libraries");
        }

        if (!hitGroup.m_IntersectionSymbol.empty() &&
            exports.find(hitGroup.m_IntersectionSymbol) == exports.end()) {
            throw std::logic_error("Intersection symbol not found in the imported DXIL libraries");
        }

        all_exports.insert(hitGroup.m_HitGroupName);
    }

    // Sanity check in debug mode: verify that the root signature associations do not reference an
    // unknown shader or hit group name
    for (const auto& assoc : m_RootSignatureAssociations) {
        for (const auto& symb : assoc.m_Symbols) {
            if (!symb.empty() && all_exports.find(symb) == all_exports.end()) {
                throw std::logic_error(
                    "Root association symbol not found in the "
                    "imported DXIL libraries and hit group names");
            }
        }
    }
#endif
    // Go through all hit groups and remove the symbols corresponding to intersection, any hit and
    // closest hit shaders from the symbol set
    for (const auto& hitGroup : m_HitGroups) {
        if (!hitGroup.m_AnyHitSymbol.empty()) {
            exports.erase(hitGroup.m_AnyHitSymbol);
        }
        if (!hitGroup.m_ClosestHitSymbol.empty()) {
            exports.erase(hitGroup.m_ClosestHitSymbol);
        }
        if (!hitGroup.m_IntersectionSymbol.empty()) {
            exports.erase(hitGroup.m_IntersectionSymbol);
        }
        exports.insert(hitGroup.m_HitGroupName);
    }

    // Finally build a vector containing ray generation and miss shaders, plus the hit group names
    for (auto&& name : exports)
        exportedSymbols.emplace_back(std::move(name));
}

RaytracingPipelineGenerator::Library::Library(const Core::Buffer&              dxil,
                                              const std::vector<std::wstring>& exportedSymbols)
    : m_Dxil(dxil),
      m_ExportedSymbols(exportedSymbols),
      m_Exports(exportedSymbols.size()),
      m_LibDesc{} {
    // Create one export descriptor per symbol
    for (size_t i = 0; i < m_ExportedSymbols.size(); i++) {
        m_Exports[i]                = {};
        m_Exports[i].Name           = m_ExportedSymbols[i].c_str();
        m_Exports[i].ExportToRename = nullptr;
        m_Exports[i].Flags          = D3D12_EXPORT_FLAG_NONE;
    }

    m_LibDesc.DXILLibrary.BytecodeLength  = dxil.GetDataSize();
    m_LibDesc.DXILLibrary.pShaderBytecode = dxil.GetData();
    m_LibDesc.NumExports                  = m_ExportedSymbols.size();
    m_LibDesc.pExports                    = m_Exports.data();
}

RaytracingPipelineGenerator::HitGroup::HitGroup(std::wstring_view hitGroupName,
                                                std::wstring_view closestHitSymbol,
                                                std::wstring_view anyHitSymbol,
                                                std::wstring_view intersectionSymbol)
    : m_HitGroupName(hitGroupName),
      m_ClosestHitSymbol(closestHitSymbol),
      m_AnyHitSymbol(anyHitSymbol),
      m_IntersectionSymbol(intersectionSymbol),
      m_Desc{} {
    m_Desc.HitGroupExport           = m_HitGroupName.c_str();
    m_Desc.ClosestHitShaderImport   = m_ClosestHitSymbol.empty() ? nullptr : m_ClosestHitSymbol.c_str();
    m_Desc.AnyHitShaderImport       = m_AnyHitSymbol.empty() ? nullptr : m_AnyHitSymbol.c_str();
    m_Desc.IntersectionShaderImport = m_IntersectionSymbol.empty() ? nullptr : m_IntersectionSymbol.c_str();
}

RaytracingPipelineGenerator::RootSignatureAssociation::RootSignatureAssociation(
    const RootSignature& rootSignature, const std::vector<std::wstring>& symbols)
    : m_RootSignature(rootSignature.GetRootSignature()),
      m_Symbols(symbols),
      m_SymbolPointers(symbols.size()),
      m_Association{} {
    for (size_t i = 0; i < m_Symbols.size(); i++)
        m_SymbolPointers[i] = m_Symbols[i].c_str();
}

}  // namespace Hitagi::Graphics::backend::DX12