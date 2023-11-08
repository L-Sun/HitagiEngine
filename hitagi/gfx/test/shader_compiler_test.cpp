#include <hitagi/gfx/shader_compiler.hpp>
#include <hitagi/gfx/utils.hpp>
#include <hitagi/utils/test.hpp>

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

const std::array shader_descs = {
    ShaderDesc{.name = "vs_shader", .type = ShaderType::Vertex, .entry = "main", .source_code = vs_shader_code},
    ShaderDesc{.name = "ps_shader", .type = ShaderType::Pixel, .entry = "main", .source_code = ps_shader_code},
    ShaderDesc{.name = "cs_shader", .type = ShaderType::Compute, .entry = "main", .source_code = cs_shader_code},
};

class ShaderCompilerTest : public testing::TestWithParam<ShaderDesc> {
protected:
    ShaderCompilerTest()
        : test_name(::testing::UnitTest::GetInstance()->current_test_info()->name()),
          compiler(test_name) {
    }

    std::pmr::string test_name;
    ShaderCompiler   compiler;
};
INSTANTIATE_TEST_SUITE_P(
    ShaderCompilerTest,
    ShaderCompilerTest,
    testing::ValuesIn(shader_descs),
    [](const testing::TestParamInfo<ShaderDesc>& info) -> std::string {
        return std::string(info.param.name);
    });

TEST_P(ShaderCompilerTest, CompileToDXIL) {
    auto result = compiler.CompileToDXIL(GetParam());
    EXPECT_FALSE(result.Empty());
}

TEST_P(ShaderCompilerTest, CompileToSPIRV) {
    auto result = compiler.CompileToSPIRV(GetParam());
    EXPECT_FALSE(result.Empty());
}

TEST_P(ShaderCompilerTest, GetShaderVertexLayout) {
    auto vertex_layout = compiler.ExtractVertexLayout({
        .name        = "vs_shader",
        .entry       = "vs_main",
        .source_code = R"""(
            struct Vertex {
                float3 position : POSITION;
                float3 normal   : NORMAL;
                float2 uv       : TEXCOORD;
            };

            float4 vs_main(Vertex v) : SV_POSITION {
                return float4(v.position, 1.0);
            }   
        )""",
    });
    EXPECT_EQ(vertex_layout.size(), 3);

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
}

int main(int argc, char** argv) {
    spdlog::set_level(spdlog::level::trace);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}