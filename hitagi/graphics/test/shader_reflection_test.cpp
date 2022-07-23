#include <hitagi/core/memory_manager.hpp>
#include <hitagi/core/file_io_manager.hpp>
#include <memory_resource>
#include <stdexcept>
#include <unordered_map>
#include "magic_enum.hpp"

#include <fmt/core.h>

#include <d3d12.h>
#include <d3d12shader.h>
#include <dxcapi.h>
#include <winerror.h>
#include <wrl/client.h>

#undef min
#undef max

using namespace hitagi;
using namespace Microsoft::WRL;

void test() {
    auto pixel_shader_source = g_FileIoManager->SyncOpenAndReadBinary("./assets/shaders/color.hlsl");

    {
        ComPtr<IDxcUtils> utils;
        DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils));

        ComPtr<IDxcCompiler3> compiler;
        DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));

        ComPtr<IDxcBlob> shader;
        std::array       compile_flags = {
                  L"./assets/shaders/color.hlsl",
                  L"-E", L"PSMain",
                  L"-T", L"ps_6_4",
                  L"-rootsig-define", L"RSDEF",
                  L"-Zi",
                  L"-Zpr",
                  L"-O0",
                  L"-Qembed_debug"};

        ComPtr<IDxcBlobEncoding> source_blob;
        utils->CreateBlobFromPinned(
            pixel_shader_source.GetData(),
            pixel_shader_source.GetDataSize(),
            0,
            &source_blob);

        DxcBuffer source;
        source.Ptr      = source_blob->GetBufferPointer();
        source.Size     = source_blob->GetBufferSize();
        source.Encoding = DXC_CP_ACP;

        ComPtr<IDxcResult> compile_result;

        auto hr = compiler->Compile(
            &source,
            compile_flags.data(), compile_flags.size(),
            nullptr,
            IID_PPV_ARGS(&compile_result));

        ComPtr<IDxcBlobUtf8> errors;
        compile_result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
        // Note that d3dcompiler would return null if no errors or warnings are present.
        // IDxcCompiler3::Compile will always return an error buffer, but its length will be zero if there are no warnings or errors.
        if (errors != nullptr && errors->GetStringLength() != 0)
            wprintf(L"Warnings and Errors:\n%S\n", errors->GetStringPointer());

        compile_result->GetStatus(&hr);
        if (FAILED(hr)) {
            wprintf(L"Compilation Failed\n");
            return;
        }

        ComPtr<IDxcBlob> pHash = nullptr;
        compile_result->GetOutput(DXC_OUT_SHADER_HASH, IID_PPV_ARGS(&pHash), nullptr);
        if (pHash != nullptr) {
            wprintf(L"Hash: ");
            auto pHashBuf = static_cast<DxcShaderHash*>(pHash->GetBufferPointer());
            for (auto digit : pHashBuf->HashDigest)
                wprintf(L"%x", digit);
            wprintf(L"\n");
        }

        ComPtr<IDxcBlob> sig_data;
        compile_result->GetOutput(DXC_OUT_ROOT_SIGNATURE, IID_PPV_ARGS(&sig_data), nullptr);
        if (sig_data != nullptr) {
            ComPtr<ID3D12VersionedRootSignatureDeserializer> deserializer;

            auto hr = D3D12CreateVersionedRootSignatureDeserializer(
                sig_data->GetBufferPointer(),
                sig_data->GetBufferSize(),
                IID_PPV_ARGS(&deserializer));
            if (SUCCEEDED(hr)) {
                const D3D12_VERSIONED_ROOT_SIGNATURE_DESC* desc = nullptr;
                deserializer->GetRootSignatureDescAtVersion(D3D_ROOT_SIGNATURE_VERSION_1_1, &desc);
                fmt::print("NumParameters:     {}\n", desc->Desc_1_1.NumParameters);
                fmt::print("NumStaticSamplers: {}\n", desc->Desc_1_1.NumStaticSamplers);
                for (std::size_t i = 0; i < desc->Desc_1_1.NumParameters; i++) {
                    auto param = desc->Desc_1_1.pParameters[i];
                    fmt::print("==== Parameter {} ====\n", i);
                    fmt::print("Visibilit: {}\n", magic_enum::enum_name(param.ShaderVisibility));
                    fmt::print("Type:      {}\n", magic_enum::enum_name(param.ParameterType));
                    if (param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) {
                        auto table = param.DescriptorTable;
                        for (std::size_t range_index = 0; range_index < table.NumDescriptorRanges; range_index++) {
                            auto range = table.pDescriptorRanges[range_index];
                            fmt::print("    ==== Range {} ====\n", range_index);
                            fmt::print("    Type:      {}\n", magic_enum::enum_name(range.RangeType));
                        }
                    }
                }
            }
        }
    }
}

int main() {
    g_MemoryManager->Initialize();
    g_FileIoManager->Initialize();

    test();

    g_FileIoManager->Finalize();
    g_MemoryManager->Finalize();

    return 0;
}