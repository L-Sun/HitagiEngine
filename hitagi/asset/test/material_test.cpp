#include <hitagi/asset/material.hpp>
#include <hitagi/core/memory_manager.hpp>
#include <hitagi/utils/test.hpp>

#include <array>

using namespace hitagi::asset;
using namespace hitagi::math;
using namespace hitagi::testing;
using namespace hitagi::utils;

TEST(MaterialTest, InitMaterial) {
    auto mat = Material::Create(
        {},
        {},
        {
            {.name = "param1", .value = vec2f{}},
            {.name = "param2", .value = vec4f{}},
            {.name = "param2", .value = vec3f{}},
            {.name = "param3", .value = vec2f{}},
            {.name = "texture", .value = std::shared_ptr<Texture>{}},
        });
    auto instance = mat->CreateInstance();

    EXPECT_TRUE(instance->GetParameter<vec2f>("param1").has_value());
    EXPECT_TRUE(instance->GetParameter<vec3f>("param2").has_value());
    EXPECT_TRUE(instance->GetParameter<vec2f>("param3").has_value());
    EXPECT_TRUE(instance->GetParameter<std::shared_ptr<Texture>>("texture").has_value());

    EXPECT_FALSE(instance->GetParameter<vec3f>("param1").has_value()) << "Can not get parameter with unmatched type.";
    EXPECT_FALSE(instance->GetParameter<vec4f>("param2").has_value()) << "Can not get override parameter.";
    EXPECT_FALSE(instance->GetParameter<vec4f>("param-no-exists").has_value()) << "Can not get no exists parameter.";
}

TEST(MaterialTest, InstanceDefaultValue) {
    auto tex = std::make_shared<Texture>(128, 128);
    auto mat = Material::Create(
        {},
        {},
        {
            {.name = "param1", .value = float{1.0f}},
            {.name = "param2", .value = vec2f{1, 2}},
            {.name = "tex1", .value = tex},
        });

    auto instance = mat->CreateInstance();

    EXPECT_EQ(instance->GetParameter<float>("param1").value(), 1.0f);
    vector_eq(instance->GetParameter<vec2f>("param2").value(), vec2f(1, 2));
    EXPECT_EQ(instance->GetParameter<decltype(tex)>("tex1").value(), tex);
    EXPECT_FALSE(instance->GetParameter<vec3f>("param-no-exists").has_value());
}

TEST(MaterialTest, ParameterLayout) {
    auto mat = Material::Create(
        {},
        {},
        {
            {.name = "param1", .value = vec2f{1, 2}},                        //  8 bytes                     =  8 bytes
            {.name = "param2", .value = float{1}},                           //  4 bytes + (4 bytes padding) = 16 bytes
            {.name = "param3", .value = vec4f{1, 2, 3, 4}},                  // 16 bytes + (0 bytes padding) = 32 bytes
            {.name = "param4", .value = std::shared_ptr<Texture>{nullptr}},  //  4 bytes + (0 bytes padding) = 36 bytes
            {.name = "param5", .value = vec2f{1, 2}},                        //  8 bytes + (4 bytes padding) = 48 bytes
            {.name = "param6", .value = vec3f{1, 2, 3}},                     // 12 bytes + (4 bytes padding) = 64 bytes
        });

    auto instance = mat->CreateInstance();
    auto buffer   = instance->GetMateriaBufferData();
    EXPECT_EQ(buffer.GetDataSize(), 64);

    vector_eq(*reinterpret_cast<vec2f*>(buffer.GetData() + 0), vec2f(1, 2));
    EXPECT_EQ(*reinterpret_cast<float*>(buffer.GetData() + 8), 1.0f);
    vector_eq(*reinterpret_cast<vec4f*>(buffer.GetData() + 16), vec4f(1, 2, 3, 4));
    EXPECT_EQ(*reinterpret_cast<std::uint32_t*>(buffer.GetData() + 32), 0);  // texture index
    vector_eq(*reinterpret_cast<vec2f*>(buffer.GetData() + 36), vec2f(1, 2));
    vector_eq(*reinterpret_cast<vec3f*>(buffer.GetData() + 48), vec3f(1, 2, 3));
}

TEST(MaterialTest, InstanceChangeValue) {
    auto tex = std::make_shared<Texture>(128, 128);
    auto mat = Material::Create(
        {},
        {},
        {
            {.name = "param1", .value = float{1.0f}},
            {.name = "param2", .value = vec2f{1, 2}},
            {.name = "tex1", .value = tex},
        });

    auto instance = mat->CreateInstance();
    {
        instance->SetParameter("param1", 2.0f);
        auto _param1 = instance->GetParameter<float>("param1");
        EXPECT_TRUE(_param1.has_value());
        EXPECT_EQ(_param1.value(), 2.0f);
    }

    {
        instance->SetParameter("param2", vec2f(3.0f, 4.0f));
        auto _param2 = instance->GetParameter<vec2f>("param2");
        EXPECT_TRUE(_param2.has_value());
        vector_eq(_param2.value(), vec2f(3.0f, 4.0f));
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}