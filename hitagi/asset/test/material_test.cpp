#include "test_macros.hpp"

#include <fstream>
#include <sstream>

import asset;
import core;
import math;
import utils;
import gfx;
import test_utils;

using namespace hitagi::asset;
using namespace hitagi::math;
using namespace hitagi::testing;
using namespace hitagi::utils;

namespace {

auto MakeMaterial(
    MaterialParameters             parameters = {},
    std::pmr::vector<MaterialPass> passes     = {},
    std::string_view               name       = "") {
    return std::make_shared<Material>(std::move(parameters), std::move(passes), name);
}

class MaterialDataDevice final : public hitagi::gfx::Device {
public:
    explicit MaterialDataDevice(hitagi::gfx::Device::Type type)
        : hitagi::gfx::Device(type, type == hitagi::gfx::Device::Type::DX12 ? "MaterialDataDeviceDX12" : "MaterialDataDeviceMock") {}

    void WaitIdle() final {}

    auto CreateFence(std::uint64_t = 0, std::string_view = "") -> std::shared_ptr<hitagi::gfx::Fence> final {
        return Unused<std::shared_ptr<hitagi::gfx::Fence>>();
    }
    auto GetCommandQueue(hitagi::gfx::CommandType) const -> hitagi::gfx::CommandQueue& final {
        return Unused<hitagi::gfx::CommandQueue&>();
    }
    auto CreateCommandContext(hitagi::gfx::CommandType, std::string_view = "") -> std::shared_ptr<hitagi::gfx::CommandContext> final {
        return Unused<std::shared_ptr<hitagi::gfx::CommandContext>>();
    }
    auto CreateSwapChain(hitagi::gfx::SwapChainDesc) -> std::shared_ptr<hitagi::gfx::SwapChain> final {
        return Unused<std::shared_ptr<hitagi::gfx::SwapChain>>();
    }
    auto CreateGPUBuffer(hitagi::gfx::GPUBufferDesc, std::span<const std::byte> = {}) -> std::shared_ptr<hitagi::gfx::GPUBuffer> final {
        return Unused<std::shared_ptr<hitagi::gfx::GPUBuffer>>();
    }
    auto CreateGPUBufferView(hitagi::gfx::GPUBufferViewDesc) -> std::shared_ptr<hitagi::gfx::GPUBufferView> final {
        return Unused<std::shared_ptr<hitagi::gfx::GPUBufferView>>();
    }
    auto CreateTexture(hitagi::gfx::TextureDesc, std::span<const std::byte> = {}) -> std::shared_ptr<hitagi::gfx::Texture> final {
        return Unused<std::shared_ptr<hitagi::gfx::Texture>>();
    }
    auto CreateTextureView(hitagi::gfx::TextureViewDesc) -> std::shared_ptr<hitagi::gfx::TextureView> final {
        return Unused<std::shared_ptr<hitagi::gfx::TextureView>>();
    }
    auto CreateSampler(hitagi::gfx::SamplerDesc) -> std::shared_ptr<hitagi::gfx::Sampler> final {
        return Unused<std::shared_ptr<hitagi::gfx::Sampler>>();
    }
    auto CreateShader(hitagi::gfx::ShaderDesc) -> std::shared_ptr<hitagi::gfx::Shader> final {
        return Unused<std::shared_ptr<hitagi::gfx::Shader>>();
    }
    auto CreateRenderPipeline(
        hitagi::gfx::RenderPipelineDesc,
        const std::pmr::vector<std::shared_ptr<hitagi::gfx::Shader>>&) -> std::shared_ptr<hitagi::gfx::RenderPipeline> final {
        return Unused<std::shared_ptr<hitagi::gfx::RenderPipeline>>();
    }
    auto CreateComputePipeline(hitagi::gfx::ComputePipelineDesc, const std::shared_ptr<hitagi::gfx::Shader>&) -> std::shared_ptr<hitagi::gfx::ComputePipeline> final {
        return Unused<std::shared_ptr<hitagi::gfx::ComputePipeline>>();
    }
    auto GetBindlessUtils() -> hitagi::gfx::BindlessUtils& final {
        return Unused<hitagi::gfx::BindlessUtils&>();
    }

private:
    template <typename T>
    [[noreturn]] static auto Unused() -> T {
        throw std::logic_error("MaterialDataDevice only supplies Device::device_type for material data generation tests.");
    }
};

auto LoadMaterialPass(Material& material, std::string_view pass_contract, hitagi::gfx::Device::Type device_type) -> const MaterialPass& {
    MaterialDataDevice device(device_type);
    material.Load({.device = device});
    const auto* pass = material.FindPass(pass_contract);
    if (!pass) throw std::logic_error("Material test pass was not found after Load().");
    return *pass;
}

auto ResolvePassTextures(const Material& material, const MaterialPass& pass) -> std::pmr::vector<std::shared_ptr<Texture>> {
    std::pmr::vector<std::shared_ptr<Texture>> textures;
    textures.reserve(pass.bindings.size());
    for (const auto& binding : pass.bindings) {
        const auto parameter = material.GetParameter<std::shared_ptr<Texture>>(binding);
        if (parameter) textures.emplace_back(*parameter);
    }
    return textures;
}

}  // namespace

TEST(MaterialTest, InitMaterial) {
    const auto texture = std::make_shared<Texture>(128, 128, hitagi::gfx::Format::R8G8B8A8_UNORM);

    MaterialParameters parameters{
        {"param1", vec2f{0.0f, 1.0f}},
        {"param2", vec4f{0.0f, 1.0f, 2.0f, 3.0f}},
        {"param2", vec3f{0.0f, 1.0f, 2.0f}},  // no effect
        {"param3", vec2f{0.0f, 1.0f}},
        {"texture", texture},
    };

    const auto mat = MakeMaterial(parameters);

    const auto material_parameters = mat->GetParameters();
    ASSERT_EQ(material_parameters.size(), 4);

    parameters.erase(std::next(parameters.begin(), 2));

    for (std::size_t i = 0; i < material_parameters.size(); ++i) {
        EXPECT_EQ(material_parameters[i], parameters[i]);
    }
}

TEST(MaterialTest, HasParameter) {
    auto mat = MakeMaterial(
        {
            {"param1", float{1.0f}},
            {"param2", vec2f{1, 2}},
            {"tex1", std::shared_ptr<Texture>{}},
        });

    EXPECT_TRUE(mat->GetParameter<float>("param1").has_value());
    EXPECT_TRUE(mat->GetParameter<vec2f>("param2").has_value());
    EXPECT_TRUE(mat->GetParameter<std::shared_ptr<Texture>>("tex1").has_value());
    EXPECT_FALSE(mat->GetParameter<int>("param1").has_value());    // Wrong type
    EXPECT_FALSE(mat->GetParameter<float>("param3").has_value());  // Non-existent parameter
}

TEST(MaterialTest, SetAndGetParameter) {
    const auto texture = std::make_shared<Texture>(128, 128, hitagi::gfx::Format::R8G8B8A8_UNORM);
    auto       mat     = MakeMaterial(
        {
            {"param1", float{1.0f}},
            {"param2", vec2f{1, 2}},
            {"tex1", texture},
        });

    mat->SetParameter("param1", 2.0f);
    mat->SetParameter("param2", vec2f(3.0f, 4.0f));
    mat->SetParameter("tex1", std::shared_ptr<Texture>{nullptr});

    EXPECT_EQ(mat->GetParameter<float>("param1").value(), 2.0f);
    EXPECT_VEC_EQ(mat->GetParameter<vec2f>("param2").value(), vec2f(3.0f, 4.0f));
    EXPECT_EQ(mat->GetParameter<std::shared_ptr<Texture>>("tex1").value(), nullptr);
    EXPECT_FALSE(mat->GetParameter<vec3f>("param1").has_value());
}

TEST(MaterialTest, MaterialBuffer_TightLayout) {
    const auto mat = MakeMaterial(
        {
            {.name = "param1", .value = vec2f{1, 2}},
            {.name = "param2", .value = float{1}},
            {.name = "param3", .value = vec4f{1, 2, 3, 4}},
            {.name = "param4", .value = std::shared_ptr<Texture>{nullptr}},
            {.name = "param5", .value = vec2f{1, 2}},
            {.name = "param6", .value = vec3f{1, 2, 3}},
        },
        {
            MaterialPass{
                .pass_contract = "Test",
                .bindings      = {"param1", "param2", "param3", "param4", "param5", "param6"},
            },
        });

    const auto& pass   = LoadMaterialPass(*mat, "Test", hitagi::gfx::Device::Type::Mock);
    const auto& buffer = pass.material_data;
    ASSERT_EQ(buffer.GetDataSize(), 64);

    EXPECT_VEC_EQ(*reinterpret_cast<const vec2f*>(buffer.GetData() + 0), vec2f(1, 2));
    EXPECT_EQ(*reinterpret_cast<const float*>(buffer.GetData() + 8), 1.0f);
    EXPECT_VEC_EQ(*reinterpret_cast<const vec4f*>(buffer.GetData() + 12), vec4f(1, 2, 3, 4));
    EXPECT_FALSE(*reinterpret_cast<const hitagi::gfx::BindlessHandle*>(buffer.GetData() + 28));
    EXPECT_VEC_EQ(*reinterpret_cast<const vec2f*>(buffer.GetData() + 44), vec2f(1, 2));
    EXPECT_VEC_EQ(*reinterpret_cast<const vec3f*>(buffer.GetData() + 52), vec3f(1, 2, 3));
}

TEST(MaterialTest, MaterialBuffer_16BitsPackingLayout) {
    const auto mat = MakeMaterial(
        {
            {.name = "param1", .value = vec2f{1, 2}},
            {.name = "param2", .value = float{1}},
            {.name = "param3", .value = vec4f{1, 2, 3, 4}},
            {.name = "param4", .value = std::shared_ptr<Texture>{nullptr}},
            {.name = "param5", .value = vec2f{1, 2}},
            {.name = "param6", .value = vec3f{1, 2, 3}},
        },
        {
            MaterialPass{
                .pass_contract = "Test",
                .bindings      = {"param1", "param2", "param3", "param4", "param5", "param6"},
            },
        });

    const auto& pass   = LoadMaterialPass(*mat, "Test", hitagi::gfx::Device::Type::DX12);
    const auto& buffer = pass.material_data;
    ASSERT_EQ(buffer.GetDataSize(), 80);

    EXPECT_VEC_EQ(*reinterpret_cast<const vec2f*>(buffer.GetData() + 0), vec2f(1, 2));
    EXPECT_EQ(*reinterpret_cast<const float*>(buffer.GetData() + 8), 1.0f);
    EXPECT_VEC_EQ(*reinterpret_cast<const vec4f*>(buffer.GetData() + 16), vec4f(1, 2, 3, 4));
    EXPECT_FALSE(*reinterpret_cast<const hitagi::gfx::BindlessHandle*>(buffer.GetData() + 32));
    EXPECT_VEC_EQ(*reinterpret_cast<const vec2f*>(buffer.GetData() + 48), vec2f(1, 2));
    EXPECT_VEC_EQ(*reinterpret_cast<const vec3f*>(buffer.GetData() + 64), vec3f(1, 2, 3));
}

TEST(MaterialTest, MaterialPass_TightBindingOrder) {
    const auto tex1 = std::make_shared<Texture>(128, 128, hitagi::gfx::Format::R8G8B8A8_UNORM);
    const auto tex2 = std::make_shared<Texture>(64, 64, hitagi::gfx::Format::R8G8B8A8_UNORM);
    const auto mat  = MakeMaterial(
        {
            {.name = "param1", .value = vec2f{1, 2}},
            {.name = "param2", .value = float{1}},
            {.name = "param3", .value = vec4f{1, 2, 3, 4}},
            {.name = "tex1", .value = tex1},
            {.name = "param4", .value = vec3f{1, 2, 3}},
            {.name = "tex2", .value = tex2},
        },
        {
            MaterialPass{
                .pass_contract = "Test",
                .bindings      = {"param1", "param2", "param3", "tex1", "param4", "tex2"},
            },
        });

    const auto& pass = LoadMaterialPass(*mat, "Test", hitagi::gfx::Device::Type::Mock);

    ASSERT_EQ(pass.bindings.size(), 6);
    EXPECT_EQ(pass.bindings[0], "param1");
    EXPECT_EQ(pass.bindings[1], "param2");
    EXPECT_EQ(pass.bindings[2], "param3");
    EXPECT_EQ(pass.bindings[3], "tex1");
    EXPECT_EQ(pass.bindings[4], "param4");
    EXPECT_EQ(pass.bindings[5], "tex2");
    EXPECT_EQ(pass.material_data.GetDataSize(), 80);

    const auto textures = ResolvePassTextures(*mat, pass);
    ASSERT_EQ(textures.size(), 2);
    EXPECT_EQ(textures[0], tex1);
    EXPECT_EQ(textures[1], tex2);
}

TEST(MaterialTest, MaterialPass_DX12BindingOrderAndMaterialData) {
    const auto mat = MakeMaterial(
        {
            {.name = "param1", .value = vec2f{1, 2}},
            {.name = "param2", .value = float{1}},
            {.name = "param3", .value = vec4f{1, 2, 3, 4}},
            {.name = "texture", .value = std::shared_ptr<Texture>{nullptr}},
            {.name = "param4", .value = vec2f{1, 2}},
            {.name = "param5", .value = vec3f{1, 2, 3}},
        },
        {
            MaterialPass{
                .pass_contract = "Test",
                .bindings      = {"param1", "param2", "param3", "texture", "param4", "param5"},
            },
        });

    const auto& pass_a = LoadMaterialPass(*mat, "Test", hitagi::gfx::Device::Type::DX12);
    ASSERT_EQ(pass_a.bindings.size(), 6);
    EXPECT_EQ(pass_a.bindings[0], "param1");
    EXPECT_EQ(pass_a.bindings[1], "param2");
    EXPECT_EQ(pass_a.bindings[2], "param3");
    EXPECT_EQ(pass_a.bindings[3], "texture");
    EXPECT_EQ(pass_a.bindings[4], "param4");
    EXPECT_EQ(pass_a.bindings[5], "param5");
    EXPECT_EQ(pass_a.material_data.GetDataSize(), 80);

    mat->Unload();
    const auto& pass_b = LoadMaterialPass(*mat, "Test", hitagi::gfx::Device::Type::DX12);
    EXPECT_EQ(pass_b.material_data.GetDataSize(), 80);

    mat->Unload();
    const auto& pass_tight = LoadMaterialPass(*mat, "Test", hitagi::gfx::Device::Type::Mock);
    EXPECT_EQ(pass_tight.material_data.GetDataSize(), 64);
}

TEST(MaterialTest, MaterialPass_GeneratesBufferAndTextures) {
    const auto tex1 = std::make_shared<Texture>(128, 128, hitagi::gfx::Format::R8G8B8A8_UNORM);
    const auto tex2 = std::make_shared<Texture>(64, 64, hitagi::gfx::Format::R8G8B8A8_UNORM);
    const auto mat  = MakeMaterial(
        {
            {.name = "param1", .value = vec2f{1, 2}},
            {.name = "param2", .value = float{3}},
            {.name = "tex1", .value = tex1},
            {.name = "tex2", .value = tex2},
        },
        {
            MaterialPass{
                .pass_contract = "Test",
                .bindings      = {"param1", "param2", "tex1", "tex2"},
            },
        });

    const auto& pass   = LoadMaterialPass(*mat, "Test", hitagi::gfx::Device::Type::DX12);
    const auto& buffer = pass.material_data;
    ASSERT_EQ(buffer.GetDataSize(), pass.material_data.GetDataSize());
    EXPECT_VEC_EQ(*reinterpret_cast<const vec2f*>(buffer.GetData() + 0), vec2f(1, 2));
    EXPECT_EQ(*reinterpret_cast<const float*>(buffer.GetData() + 8), 3.0f);
    EXPECT_FALSE(*reinterpret_cast<const hitagi::gfx::BindlessHandle*>(buffer.GetData() + 16));
    EXPECT_FALSE(*reinterpret_cast<const hitagi::gfx::BindlessHandle*>(buffer.GetData() + 32));

    const auto textures = ResolvePassTextures(*mat, pass);
    ASSERT_EQ(textures.size(), 2);
    EXPECT_EQ(textures[0], tex1);
    EXPECT_EQ(textures[1], tex2);
}

TEST(MaterialTest, MaterialLoad_WritesTextureViewBindlessHandle) {
    auto             device = hitagi::gfx::create_device(hitagi::gfx::Device::Type::Mock, "MaterialLoadTextureView");
    const std::array pixels{
        static_cast<std::byte>(255),
        static_cast<std::byte>(255),
        static_cast<std::byte>(255),
        static_cast<std::byte>(255),
    };
    const auto texture = std::make_shared<Texture>(
        1,
        1,
        hitagi::gfx::Format::R8G8B8A8_UNORM,
        hitagi::core::Buffer(pixels.size(), pixels.data()),
        "white");
    const auto mat = MakeMaterial(
        {
            {.name = "albedo", .value = texture},
        },
        {
            MaterialPass{
                .pass_contract = "Test",
                .bindings      = {"albedo"},
            },
        });

    mat->Load({.device = *device});

    ASSERT_NE(texture->GetGPUData(), nullptr);
    ASSERT_NE(texture->GetGPUView(), nullptr);
    const auto expected_handle = texture->GetGPUView()->GetBindlessHandle();
    ASSERT_TRUE(expected_handle);

    const auto passes = mat->GetPasses();
    ASSERT_EQ(passes.size(), 1);
    ASSERT_EQ(passes[0].material_data.GetDataSize(), 16);

    const auto stored_handle = *reinterpret_cast<const hitagi::gfx::BindlessHandle*>(passes[0].material_data.GetData());
    EXPECT_EQ(stored_handle.index, expected_handle.index);
    EXPECT_EQ(stored_handle.type, expected_handle.type);
    EXPECT_EQ(stored_handle.writable, expected_handle.writable);
    EXPECT_EQ(stored_handle.version, expected_handle.version);
}
