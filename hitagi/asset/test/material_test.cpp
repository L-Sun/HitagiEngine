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

auto LoadMaterialPass(Material& material, std::string_view pass_contract) -> const MaterialPass& {
    auto device = hitagi::gfx::create_device(hitagi::gfx::Device::Type::Mock, "MaterialDataDevice");
    material.Load({.device = *device});
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
        {.name = "param1", .value = vec2f{0.0f, 1.0f}},
        {.name = "param2", .value = vec4f{0.0f, 1.0f, 2.0f, 3.0f}},
        {.name = "param2", .value = vec3f{0.0f, 1.0f, 2.0f}},  // no effect
        {.name = "param3", .value = vec2f{0.0f, 1.0f}},
        {.name = "texture", .value = texture},
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
            {.name = "param1", .value = float{1.0f}},
            {.name = "param2", .value = vec2f{1, 2}},
            {.name = "tex1", .value = std::shared_ptr<Texture>{}},
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
            {.name = "param1", .value = float{1.0f}},
            {.name = "param2", .value = vec2f{1, 2}},
            {.name = "tex1", .value = texture},
        });

    mat->SetParameter("param1", 2.0f);
    mat->SetParameter("param2", vec2f(3.0f, 4.0f));
    mat->SetParameter("tex1", std::shared_ptr<Texture>{nullptr});

    EXPECT_EQ(mat->GetParameter<float>("param1").value(), 2.0f);
    EXPECT_VEC_EQ(mat->GetParameter<vec2f>("param2").value(), vec2f(3.0f, 4.0f));
    EXPECT_EQ(mat->GetParameter<std::shared_ptr<Texture>>("tex1").value(), nullptr);
    EXPECT_FALSE(mat->GetParameter<vec3f>("param1").has_value());
}

TEST(MaterialTest, DefineParameterFixesType) {
    Material material;
    material.DefineParameter<float>("roughness", 0.5f);
    material.SetParameter("roughness", 0.8f);
    EXPECT_FLOAT_EQ(material.GetParameter<float>("roughness").value(), 0.8f);

    EXPECT_THROW(material.DefineParameter("roughness", 0.2f), std::invalid_argument);
    EXPECT_THROW(material.DefineParameter("roughness", vec3f{1, 0, 0}), std::invalid_argument);
    EXPECT_THROW(material.SetParameter("roughness", vec3f{1, 0, 0}), std::invalid_argument);
    EXPECT_THROW(material.SetParameter("unknown", 1.0f), std::invalid_argument);

    ASSERT_EQ(material.GetParameters().size(), 1);
    EXPECT_FLOAT_EQ(material.GetParameter<float>("roughness").value(), 0.8f);
    EXPECT_FALSE(material.GetParameter<vec3f>("roughness").has_value());
}

TEST(MaterialTest, RejectedAssignmentPreservesLoadedData) {
    Material    material({{.name = "roughness", .value = 0.5f}}, {{.pass_contract = "Forward", .bindings = {"roughness"}}});
    const auto& pass = LoadMaterialPass(material, "Forward");
    ASSERT_FALSE(pass.material_data.Empty());
    const auto size = pass.material_data.GetDataSize();

    EXPECT_THROW(material.SetParameter("roughness", vec3f{1, 0, 0}), std::invalid_argument);
    EXPECT_THROW(material.SetParameter("unknown", 1.0f), std::invalid_argument);
    EXPECT_THROW(material.DefineParameter("roughness", 0.2f), std::invalid_argument);
    EXPECT_EQ(material.GetLoadState(), ResourceLoadState::Loaded);
    EXPECT_EQ(pass.material_data.GetDataSize(), size);
    EXPECT_FLOAT_EQ(*reinterpret_cast<const float*>(pass.material_data.GetData()), 0.5f);

    material.SetParameter("roughness", 0.8f);
    EXPECT_EQ(material.GetLoadState(), ResourceLoadState::Unloaded);
    EXPECT_TRUE(pass.material_data.Empty());
    const auto& updated = LoadMaterialPass(material, "Forward");
    EXPECT_FLOAT_EQ(*reinterpret_cast<const float*>(updated.material_data.GetData()), 0.8f);

    material.DefineParameter("metallic", 1.0f);
    EXPECT_EQ(material.GetLoadState(), ResourceLoadState::Unloaded);
    EXPECT_TRUE(updated.material_data.Empty());
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

    const auto& pass   = LoadMaterialPass(*mat, "Test");
    const auto& buffer = pass.material_data;
    ASSERT_EQ(buffer.GetDataSize(), 64);

    EXPECT_VEC_EQ(*reinterpret_cast<const vec2f*>(buffer.GetData() + 0), vec2f(1, 2));
    EXPECT_EQ(*reinterpret_cast<const float*>(buffer.GetData() + 8), 1.0f);
    EXPECT_VEC_EQ(*reinterpret_cast<const vec4f*>(buffer.GetData() + 12), vec4f(1, 2, 3, 4));
    EXPECT_FALSE(*reinterpret_cast<const hitagi::gfx::BindlessHandle*>(buffer.GetData() + 28));
    EXPECT_VEC_EQ(*reinterpret_cast<const vec2f*>(buffer.GetData() + 44), vec2f(1, 2));
    EXPECT_VEC_EQ(*reinterpret_cast<const vec3f*>(buffer.GetData() + 52), vec3f(1, 2, 3));
}

TEST(MaterialTest, EncodeMaterialDataWithoutDevice) {
    const hitagi::gfx::BindlessHandle      handle{.index = 37, .type = hitagi::gfx::BindlessHandleType::Texture, .version = 2};
    const std::array<MaterialDataValue, 5> values{vec2f{1, 2}, 3.0f, vec4f{4, 5, 6, 7}, handle, vec3f{8, 9, 10}};
    const auto                             buffer = EncodeMaterialData(values);
    ASSERT_EQ(buffer.GetDataSize(), 56);
    const std::array<float, 7> scalars{1, 2, 3, 4, 5, 6, 7};
    EXPECT_EQ(std::memcmp(buffer.GetData(), scalars.data(), sizeof(scalars)), 0);
    EXPECT_EQ(std::memcmp(buffer.GetData() + 28, &handle, sizeof(handle)), 0);
    const vec3f tail{8, 9, 10};
    EXPECT_EQ(std::memcmp(buffer.GetData() + 44, &tail, sizeof(tail)), 0);
    EXPECT_TRUE(EncodeMaterialData({}).Empty());
}

TEST(MaterialTest, EncodeMaterialMatrixAfterScalar) {
    const mat4f                            matrix{{1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10, 11, 12}, {13, 14, 15, 16}};
    const std::array<MaterialDataValue, 2> values{2.0f, matrix};
    const auto                             buffer = EncodeMaterialData(values);
    ASSERT_EQ(buffer.GetDataSize(), 68);
    EXPECT_EQ(std::memcmp(buffer.GetData() + 4, &matrix, sizeof(matrix)), 0);
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

    const auto& pass = LoadMaterialPass(*mat, "Test");

    ASSERT_EQ(pass.bindings.size(), 6);
    EXPECT_EQ(pass.bindings[0], "param1");
    EXPECT_EQ(pass.bindings[1], "param2");
    EXPECT_EQ(pass.bindings[2], "param3");
    EXPECT_EQ(pass.bindings[3], "tex1");
    EXPECT_EQ(pass.bindings[4], "param4");
    EXPECT_EQ(pass.bindings[5], "tex2");
    EXPECT_EQ(pass.material_data.GetDataSize(), 72);

    const auto textures = ResolvePassTextures(*mat, pass);
    ASSERT_EQ(textures.size(), 2);
    EXPECT_EQ(textures[0], tex1);
    EXPECT_EQ(textures[1], tex2);
}

TEST(MaterialTest, MaterialPass_BindingOrderAndReload) {
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

    const auto& pass_a = LoadMaterialPass(*mat, "Test");
    ASSERT_EQ(pass_a.bindings.size(), 6);
    EXPECT_EQ(pass_a.bindings[0], "param1");
    EXPECT_EQ(pass_a.bindings[1], "param2");
    EXPECT_EQ(pass_a.bindings[2], "param3");
    EXPECT_EQ(pass_a.bindings[3], "texture");
    EXPECT_EQ(pass_a.bindings[4], "param4");
    EXPECT_EQ(pass_a.bindings[5], "param5");
    EXPECT_EQ(pass_a.material_data.GetDataSize(), 64);

    mat->Unload();
    const auto& pass_b = LoadMaterialPass(*mat, "Test");
    EXPECT_EQ(pass_b.material_data.GetDataSize(), 64);

    mat->Unload();
    const auto& pass_tight = LoadMaterialPass(*mat, "Test");
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

    const auto& pass   = LoadMaterialPass(*mat, "Test");
    const auto& buffer = pass.material_data;
    ASSERT_EQ(buffer.GetDataSize(), pass.material_data.GetDataSize());
    EXPECT_VEC_EQ(*reinterpret_cast<const vec2f*>(buffer.GetData() + 0), vec2f(1, 2));
    EXPECT_EQ(*reinterpret_cast<const float*>(buffer.GetData() + 8), 3.0f);
    EXPECT_FALSE(*reinterpret_cast<const hitagi::gfx::BindlessHandle*>(buffer.GetData() + 12));
    EXPECT_FALSE(*reinterpret_cast<const hitagi::gfx::BindlessHandle*>(buffer.GetData() + 28));

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
