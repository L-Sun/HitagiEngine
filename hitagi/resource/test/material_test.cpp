#include <hitagi/resource/material.hpp>
#include <hitagi/utils/test.hpp>

#include <array>
#include "gtest/gtest.h"

using namespace hitagi::resource;
using namespace hitagi::math;
using namespace hitagi::testing;

TEST(MaterialTest, InitMaterial) {
    auto mat = Material::Builder()
                   .Type(MaterialType::Custom)
                   .AppendParameterInfo<vec2f>("param1")
                   .AppendParameterInfo<vec4f>("param2")
                   .AppendParameterInfo<vec2f>("param3")
                   .AppendParameterArrayInfo<vec2f, 3>("param4")
                   .AppendTextureName("texture")
                   .Build();

    EXPECT_EQ(mat->GetType(), MaterialType::Custom);
    EXPECT_TRUE(mat->IsValidParameter<vec2f>("param1"));
    EXPECT_TRUE(mat->IsValidParameter<vec4f>("param2"));
    EXPECT_TRUE(mat->IsValidParameter<vec2f>("param3"));
    EXPECT_TRUE((mat->IsValidParameter<std::array<vec2f, 3>>("param4")));
    EXPECT_TRUE(mat->IsValidTextureParameter("texture"));

    EXPECT_FALSE(mat->IsValidParameter<vec3f>("param1"));
    EXPECT_FALSE(mat->IsValidParameter<vec4f>("param-no-exists"));
    EXPECT_FALSE((mat->IsValidParameter<std::array<vec2f, 4>>("param4")));
    EXPECT_FALSE(mat->IsValidTextureParameter("texture-no-exists"));
}

TEST(MaterialTest, ParameterLayout) {
    auto mat = Material::Builder()
                   .Type(MaterialType::Custom)
                   .AppendParameterInfo<vec2f>("param1")
                   .AppendParameterInfo<float>("param2")
                   .AppendParameterInfo<vec4f>("param3")
                   .AppendParameterInfo<vec2f>("param4")
                   .AppendParameterArrayInfo<vec2f, 3>("param5")
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

TEST(MaterialTest, Instance) {
    auto mat = Material::Builder()
                   .Type(MaterialType::Custom)
                   .AppendParameterInfo<vec2f>("param1")
                   .AppendParameterArrayInfo<vec4f, 2>("param2")
                   .AppendTextureName("texture")
                   .Build();

    vec2f                param1{0.0f, 1.0f};
    std::array<vec4f, 2> param2{{{0.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 1.0f, 0.0f}}};

    auto instance = mat->CreateInstance();
    (*instance)
        .SetParameter("param1", param1)
        .SetParameter("param2", param2);

    {
        auto _param1 = instance->GetValue<vec2f>("param1");
        EXPECT_TRUE(_param1.has_value());
        vector_near(*_param1, param1, 1e-4);
    }

    {
        auto _param2 = instance->GetValue<vec4f, 2>("param2");
        EXPECT_TRUE(_param2.has_value());
        for (std::size_t index = 0; index < 2; index++) {
            vector_near((*_param2)[index], param2[index], 1e-4);
        }
    }

    EXPECT_FALSE((instance->GetValue<vec2f>("param-no-exists").has_value()));
    EXPECT_FALSE((instance->GetValue<vec3f>("param1").has_value()));
    EXPECT_FALSE((instance->GetValue<vec4f, 3>("param2").has_value()));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}