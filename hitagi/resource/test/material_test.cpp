#include <hitagi/resource/material.hpp>
#include <hitagi/resource/material_instance.hpp>
#include <hitagi/utils/test.hpp>

#include <array>
#include "gtest/gtest.h"
#include "hitagi/core/memory_manager.hpp"

using namespace hitagi::resource;
using namespace hitagi::math;
using namespace hitagi::testing;

TEST(MaterialTest, InitMaterial) {
    auto mat = Material::Builder()
                   .AppendParameterInfo<vec2f>("param1")
                   .AppendParameterInfo<vec4f>("param2")
                   .AppendParameterInfo<vec2f>("param3")
                   .AppendParameterArrayInfo<vec2f>("param4", 3)
                   .AppendTextureName("texture")
                   .Build();

    EXPECT_TRUE(mat->IsValidParameter<vec2f>("param1"));
    EXPECT_TRUE(mat->IsValidParameter<vec4f>("param2"));
    EXPECT_TRUE(mat->IsValidParameter<vec2f>("param3"));
    EXPECT_TRUE((mat->IsValidParameterArray<vec2f>("param4", 3)));
    EXPECT_TRUE(mat->IsValidTextureParameter("texture"));

    EXPECT_FALSE(mat->IsValidParameter<vec3f>("param1"));
    EXPECT_FALSE(mat->IsValidParameter<vec4f>("param-no-exists"));
    EXPECT_FALSE(mat->IsValidParameterArray<vec2f>("param4", 4));
    EXPECT_FALSE(mat->IsValidTextureParameter("texture-no-exists"));
}

TEST(MaterialTest, ParameterLayout) {
    auto mat = Material::Builder()
                   .AppendParameterInfo<vec2f>("param1")
                   .AppendParameterInfo<float>("param2")
                   .AppendParameterInfo<vec4f>("param3")
                   .AppendParameterInfo<vec2f>("param4")
                   .AppendParameterArrayInfo<vec2f>("param5", 3)
                   .AppendParameterInfo<vec3f>("param6")
                   .Build();

    EXPECT_EQ(mat->GetParameterInfo("param1")->offset, 0);
    EXPECT_EQ(mat->GetParameterInfo("param2")->offset, 8);
    EXPECT_EQ(mat->GetParameterInfo("param3")->offset, 16);
    EXPECT_EQ(mat->GetParameterInfo("param4")->offset, 32);
    EXPECT_EQ(mat->GetParameterInfo("param5")->offset, 48);
    EXPECT_EQ(mat->GetParameterInfo("param6")->offset, 80);

    EXPECT_EQ(mat->GetParameterInfo("param1")->size, sizeof(vec2f));
    EXPECT_EQ(mat->GetParameterInfo("param2")->size, sizeof(float));
    EXPECT_EQ(mat->GetParameterInfo("param3")->size, sizeof(vec4f));
    EXPECT_EQ(mat->GetParameterInfo("param4")->size, sizeof(vec2f));
    EXPECT_EQ(mat->GetParameterInfo("param5")->size, sizeof(vec2f) * 3);
    EXPECT_EQ(mat->GetParameterInfo("param6")->size, sizeof(vec3f));
}
TEST(MaterialTest, InstanceDefaultValue) {
    vec2f                   param1{0.0f, 1.0f};
    std::pmr::vector<vec4f> param2 = {{{0.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 1.0f, 0.0f}}};

    auto mat = Material::Builder()
                   .AppendParameterInfo<vec2f>("param1", param1)
                   .AppendParameterArrayInfo<vec4f>("param2", 2, param2)
                   .AppendTextureName("texture", "test.png")
                   .Build();

    auto instance = mat->CreateInstance();
    {
        auto _param1 = instance->GetValue<vec2f>("param1");
        EXPECT_TRUE(_param1.has_value());
        vector_eq(*_param1, param1);
    }

    {
        auto _param2 = instance->GetValue<vec4f, 2>("param2");
        EXPECT_TRUE(_param2.has_value());
        for (std::size_t index = 0; index < 2; index++) {
            vector_eq((*_param2)[index], param2[index]);
        }
    }
    {
        auto _param3 = instance->GetTexture("texture");
        EXPECT_STREQ(_param3->GetTexturePath().string().c_str(), "test.png");
    }
}

TEST(MaterialTest, InstanceChangeValue) {
    auto mat = Material::Builder()
                   .AppendParameterInfo<vec2f>("param1")
                   .AppendParameterArrayInfo<vec4f>("param2", 2)
                   .AppendTextureName("texture", "test.png")
                   .Build();

    vec2f                   param1 = {0.0f, 1.0f};
    std::pmr::vector<vec4f> param2 = {{{1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 0.0f}}};

    auto instance = mat->CreateInstance();
    (*instance)
        .SetParameter("param1", param1)
        .SetParameter("param2", param2);
    {
        auto _param1 = instance->GetValue<vec2f>("param1");
        EXPECT_TRUE(_param1.has_value());
        vector_eq(*_param1, param1);
    }

    {
        auto _param2 = instance->GetValue<vec4f, 2>("param2");
        EXPECT_TRUE(_param2.has_value());
        for (std::size_t index = 0; index < 2; index++) {
            vector_eq((*_param2)[index], param2[index]);
        }
    }

    EXPECT_FALSE((instance->GetValue<vec2f>("param-no-exists").has_value()));
    EXPECT_FALSE((instance->GetValue<vec3f>("param1").has_value()));
    EXPECT_FALSE((instance->GetValue<vec4f, 3>("param2").has_value()));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    hitagi::g_MemoryManager->Initialize();

    int result = RUN_ALL_TESTS();

    hitagi::g_MemoryManager->Finalize();
    return result;
}