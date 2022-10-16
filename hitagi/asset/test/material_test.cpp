#include <hitagi/asset/material.hpp>
#include <hitagi/core/memory_manager.hpp>
#include <hitagi/utils/test.hpp>

#include <array>

using namespace hitagi::asset;
using namespace hitagi::math;
using namespace hitagi::testing;
using namespace hitagi::utils;

TEST(MaterialTest, InitMaterial) {
    auto mat = Material::Builder()
                   .AppendParameterInfo<vec2f>("param1")
                   .AppendParameterInfo<vec4f>("param2")
                   .AppendParameterInfo<vec2f>("param3")
                   .AppendParameterInfo<std::shared_ptr<Texture>>("texture")
                   .Build();

    EXPECT_TRUE(mat->IsValidParameter<vec2f>("param1"));
    EXPECT_TRUE(mat->IsValidParameter<vec4f>("param2"));
    EXPECT_TRUE(mat->IsValidParameter<vec2f>("param3"));
    EXPECT_TRUE(mat->IsValidParameter<std::shared_ptr<Texture>>("texture"));

    EXPECT_FALSE(mat->IsValidParameter<vec3f>("param1"));
    EXPECT_FALSE(mat->IsValidParameter<vec4f>("param-no-exists"));
}

TEST(MaterialTest, ParameterLayout) {
    auto mat = Material::Builder()
                   .AppendParameterInfo("param1", vec2f(1, 2))                        //  8 bytes                     =  8 bytes
                   .AppendParameterInfo("param2", 1.0f)                               //  4 bytes + (4 bytes padding) = 16 bytes
                   .AppendParameterInfo("param3", vec4f(1, 2, 3, 4))                  // 16 bytes + (0 bytes padding) = 32 bytes
                   .AppendParameterInfo("param4", std::shared_ptr<Texture>(nullptr))  //  4 bytes + (0 bytes padding) = 36 bytes
                   .AppendParameterInfo("param5", vec2f(1, 2))                        //  8 bytes + (4 bytes padding) = 48 bytes
                   .AppendParameterInfo("param6", vec3f(1, 2, 3))                     // 12 bytes + (4 bytes padding) = 64 bytes
                   .Build();

    EXPECT_EQ(mat->GetParametersBufferSize(), 64);
    auto instance = mat->CreateInstance();
    auto buffer   = instance->GetParameterBuffer();

    vector_eq(*reinterpret_cast<vec2f*>(buffer.GetData() + 0), vec2f(1, 2));
    EXPECT_EQ(*reinterpret_cast<float*>(buffer.GetData() + 8), 1.0f);
    vector_eq(*reinterpret_cast<vec4f*>(buffer.GetData() + 16), vec4f(1, 2, 3, 4));
    EXPECT_EQ(*reinterpret_cast<std::uint32_t*>(buffer.GetData() + 32), -1);  // texture index
    vector_eq(*reinterpret_cast<vec2f*>(buffer.GetData() + 36), vec2f(1, 2));
    vector_eq(*reinterpret_cast<vec3f*>(buffer.GetData() + 48), vec3f(1, 2, 3));
}

TEST(MaterialTest, InstanceDefaultValue) {
    auto texture = std::make_shared<Texture>();
    auto mat     = Material::Builder()
                   .AppendParameterInfo("param1", 1.0f)
                   .AppendParameterInfo("param2", vec2f(1, 2))
                   .AppendParameterInfo("tex1", texture)
                   .Build();

    auto instance = mat->CreateInstance();
    EXPECT_EQ(instance->GetParameter<float>("param1").value(), 1.0f);
    vector_eq(instance->GetParameter<vec2f>("param2").value(), vec2f(1, 2));
    EXPECT_EQ(instance->GetParameter<decltype(texture)>("tex1").value(), texture);
    EXPECT_FALSE(instance->GetParameter<vec3f>("param-no-exists").has_value());
}

TEST(MaterialTest, InstanceChangeValue) {
    auto texture = std::make_shared<Texture>();
    auto mat     = Material::Builder()
                   .AppendParameterInfo("param1", 1.0f)
                   .AppendParameterInfo("param2", vec2f(1, 2))
                   .AppendParameterInfo("tex1", texture)
                   .Build();

    auto instance = mat->CreateInstance();
    {
        EXPECT_TRUE(instance->SetParameter("param1", 2.0f));
        auto _param1 = instance->GetParameter<float>("param1");
        EXPECT_TRUE(_param1.has_value());
        EXPECT_EQ(_param1.value(), 2.0f);
    }

    {
        EXPECT_TRUE(instance->SetParameter("param2", vec2f(3.0f, 4.0f)));
        auto _param2 = instance->GetParameter<vec2f>("param2");
        EXPECT_TRUE(_param2.has_value());
        vector_eq(_param2.value(), vec2f(3.0f, 4.0f));
    }

    {
        EXPECT_FALSE(instance->SetParameter("param1", vec2f(3.0f, 4.0f)));
        EXPECT_FALSE(instance->SetParameter("param2", 3.0f));
        EXPECT_FALSE(instance->SetParameter("param-no-exists", 3.0f));
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}