#include "test_macros.hpp"
import interop.gtest;

import std;
import gfx;
import test_utils;

using namespace hitagi::gfx;

constexpr auto vs_shader_code = R"""(
struct Constant {
    float value;
};

[[vk::push_constant]]
ConstantBuffer<Constant> value;

float4 main() : POSITION {
    return float4(value.value, 0.0, 0.0, 1.0);
}
)""";

constexpr auto ps_shader_code = R"""(
struct Constant {
    float value;
};

[[vk::push_constant]]
ConstantBuffer<Constant> value;

float4 main() : SV_TARGET {
    return float4(value.value, 0.0, 0.0, 1.0);
}
)""";

constexpr auto cs_shader_code = R"""(
struct Constant {
    float value;
};
[[vk::push_constant]]
ConstantBuffer<Constant> value;
RWBuffer<float> buffer : register(u0, space0);
[numthreads(1, 1, 1)]
void main() {
    buffer[0] = value.value;
}
)""";

struct ShaderTestDesc {
    std::string_view name;
    ShaderType       type;
    std::string_view source_code;
};

constexpr std::array shader_descs = {
    ShaderTestDesc{.name = "vs_shader", .type = ShaderType::Vertex, .source_code = vs_shader_code},
    ShaderTestDesc{.name = "ps_shader", .type = ShaderType::Pixel, .source_code = ps_shader_code},
    ShaderTestDesc{.name = "cs_shader", .type = ShaderType::Compute, .source_code = cs_shader_code},
};

static auto ToShaderDesc(ShaderTestDesc desc) -> ShaderDesc {
    return {
        .name    = std::pmr::string(desc.name),
        .type    = desc.type,
        .entry   = "main",
        .program = {.modules = {{.source_code = std::pmr::string(desc.source_code)}}},
    };
}

class ShaderCompilerTest : public testing::TestWithParam<ShaderTestDesc> {
protected:
    ShaderCompilerTest()
        : test_name(::testing::UnitTest::GetInstance()->current_test_info()->name()),
          compiler(test_name) {
    }

    std::string    test_name;
    ShaderCompiler compiler;
};
INSTANTIATE_TEST_SUITE_P(
    ShaderCompilerTest,
    ShaderCompilerTest,
    testing::ValuesIn(shader_descs),
    [](const testing::TestParamInfo<ShaderTestDesc>& info) -> std::string {
        return std::string(info.param.name);
    });

TEST_P(ShaderCompilerTest, CompileToDXIL) {
    const auto desc   = ToShaderDesc(GetParam());
    auto       result = compiler.LoadProgram(desc.program, ShaderTarget::DXIL)->Compile(desc.entry, desc.type);
    EXPECT_FALSE(result.bytecode.Empty());
}

TEST_P(ShaderCompilerTest, CompileToSPIRV) {
    const auto desc   = ToShaderDesc(GetParam());
    auto       result = compiler.LoadProgram(desc.program, ShaderTarget::SPIRV)->Compile(desc.entry, desc.type);
    EXPECT_FALSE(result.bytecode.Empty());
}

TEST_P(ShaderCompilerTest, GetShaderVertexLayout) {
    const ShaderDesc desc{
        .name    = "vs_shader",
        .type    = ShaderType::Vertex,
        .entry   = "vs_main",
        .program = {.modules = {{.source_code = R"""(
            struct Vertex {
                float3 position : POSITION;
                float3 normal   : NORMAL;
                float2 uv       : TEXCOORD;
                uint vertex_id  : SV_VertexID;
            };

            float4 vs_main(Vertex v) : SV_POSITION {
                return float4(v.position, 1.0);
            }   
        )"""}}},
    };
    auto vertex_layout = compiler.LoadProgram(desc.program, ShaderTarget::SPIRV)->Compile(desc.entry, desc.type).vertex_layout;
    ASSERT_EQ(vertex_layout.size(), 3);

    EXPECT_STREQ(vertex_layout[0].semantic.c_str(), "POSITION0");
    EXPECT_EQ(vertex_layout[0].binding, 0);
    EXPECT_EQ(vertex_layout[0].format, Format::R32G32B32_FLOAT);
    EXPECT_EQ(vertex_layout[0].offset, 0);
    EXPECT_EQ(vertex_layout[0].stride, get_format_byte_size(Format::R32G32B32_FLOAT));

    EXPECT_STREQ(vertex_layout[1].semantic.c_str(), "NORMAL0");
    EXPECT_EQ(vertex_layout[1].binding, 1);
    EXPECT_EQ(vertex_layout[1].format, Format::R32G32B32_FLOAT);
    EXPECT_EQ(vertex_layout[1].offset, 0);
    EXPECT_EQ(vertex_layout[1].stride, get_format_byte_size(Format::R32G32B32_FLOAT));

    EXPECT_STREQ(vertex_layout[2].semantic.c_str(), "TEXCOORD0");
    EXPECT_EQ(vertex_layout[2].binding, 2);
    EXPECT_EQ(vertex_layout[2].format, Format::R32G32_FLOAT);
    EXPECT_EQ(vertex_layout[2].offset, 0);
    EXPECT_EQ(vertex_layout[2].stride, get_format_byte_size(Format::R32G32_FLOAT));
    EXPECT_EQ(vertex_layout[0].location, 0);
    EXPECT_EQ(vertex_layout[1].location, 1);
    EXPECT_EQ(vertex_layout[2].location, 2);
}

TEST(ShaderProgramTest, ComposesModulesAndSpecializesEntryPointsForBothTargets) {
    const ShaderProgramDesc desc{.modules = {
                                     {.name = "material_contract", .source_code = R"(
            public interface IMaterial { float4 evaluate(); }
            public struct Red : IMaterial { public float4 evaluate() { return float4(1, 0, 0, 1); } }
            public struct Green : IMaterial { public float4 evaluate() { return float4(0, 1, 0, 1); } }
        )"},
                                     {.name = "material_pass", .source_code = R"(
            import material_contract;
            float4 shade<T : IMaterial>() : SV_Target { T material; return material.evaluate(); }
            float4 vertex(float3 position : POSITION) : SV_Position { return float4(position, 1); }
        )"},
                                 }};
    for (auto target : {ShaderTarget::DXIL, ShaderTarget::SPIRV}) {
        std::shared_ptr<ShaderProgram> program;
        {
            ShaderCompiler compiler("ModuleSpecialization");
            program = compiler.LoadProgram(desc, target);
        }
        // Programs retain their global session even after the compiler owner goes away.
        for (const auto* type : {"Red", "Green"}) {
            const std::array<std::pmr::string, 1> types{type};
            EXPECT_FALSE(program->Compile("shade", ShaderType::Pixel, types).bytecode.Empty());
        }
        EXPECT_EQ(program->Compile("vertex", ShaderType::Vertex).vertex_layout.size(), 1);
        const std::array<std::pmr::string, 1> missing{"MissingMaterial"};
        EXPECT_THROW(program->Compile("shade", ShaderType::Pixel, missing), std::invalid_argument);
        EXPECT_THROW(program->Compile("missing_entry", ShaderType::Pixel), std::runtime_error);
    }
}

TEST(ShaderProgramTest, RejectsInvalidProgramsWithoutPoisoningTheCompiler) {
    ShaderCompiler compiler("InvalidPrograms");
    EXPECT_THROW(compiler.LoadProgram({}, ShaderTarget::SPIRV), std::invalid_argument);
    const ShaderProgramDesc invalid{.modules = {{.name = "broken", .source_code = "this is not a shader;"}}};
    EXPECT_THROW(compiler.LoadProgram(invalid, ShaderTarget::SPIRV), std::runtime_error);
    const ShaderProgramDesc duplicate{.modules = {
                                          {.name = "same", .source_code = "float value() { return 1; }"},
                                          {.name = "same", .source_code = "float value() { return 2; }"},
                                      }};
    EXPECT_THROW(compiler.LoadProgram(duplicate, ShaderTarget::SPIRV), std::invalid_argument);
    const auto desc = ToShaderDesc(shader_descs[1]);
    EXPECT_FALSE(compiler.LoadProgram(desc.program, ShaderTarget::SPIRV)->Compile(desc.entry, desc.type).bytecode.Empty());
}

TEST(ShaderProgramTest, ReflectsComputeThreadGroupSizesPerEntryPoint) {
    ShaderCompiler compiler("ComputeGroupReflection");
    const ShaderProgramDesc desc{.modules = {{.name = "compute_groups", .source_code = R"(
        RWStructuredBuffer<uint> output;
        [shader("compute")][numthreads(8, 4, 2)]
        void volume(uint3 id : SV_DispatchThreadID) { output[id.x] = id.y + id.z; }
        [shader("compute")][numthreads(64, 1, 1)]
        void skin(uint3 id : SV_DispatchThreadID) { output[id.x] = id.x; }
    )"}}};
    for (auto target : {ShaderTarget::DXIL, ShaderTarget::SPIRV, ShaderTarget::Metal}) {
        auto program = compiler.LoadProgram(desc, target);
        const auto volume = program->Compile("volume", ShaderType::Compute);
        EXPECT_EQ(volume.thread_group_size.x, 8u);
        EXPECT_EQ(volume.thread_group_size.y, 4u);
        EXPECT_EQ(volume.thread_group_size.z, 2u);
        const auto skin = program->Compile("skin", ShaderType::Compute);
        EXPECT_EQ(skin.thread_group_size.x, 64u);
        EXPECT_EQ(skin.thread_group_size.y, 1u);
        EXPECT_EQ(skin.thread_group_size.z, 1u);
    }
}
