#include <hitagi/gfx/shader_compiler.hpp>
#include <hitagi/utils/test.hpp>

using namespace hitagi::gfx;

constexpr std::string_view vs_shader_code = R"""(
struct Constant {
    float value;
};

[[vk::push_constant]]
ConstantBuffer<Constant> value;

float4 main() : POSITION {
    return float4(value.value, 0.0, 0.0, 1.0);
}
)""";

constexpr std::string_view ps_shader_code = R"""(
struct Constant {
    float value;
};

[[vk::push_constant]]
ConstantBuffer<Constant> value;

float4 main() : SV_TARGET {
    return float4(value.value, 0.0, 0.0, 1.0);
}
)""";

constexpr std::string_view cs_shader_code = R"""(
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

constexpr std::array shader_descs = {
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

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}