#include <hitagi/asset/material.hpp>
#include <hitagi/core/memory_manager.hpp>
#include <hitagi/utils/test.hpp>

using namespace hitagi::asset;
using namespace hitagi::math;
using namespace hitagi::testing;
using namespace hitagi::utils;

TEST(MaterialTest, InitMaterial) {
    const auto texture = std::make_shared<Texture>(128, 128, hitagi::gfx::Format::R8G8B8A8_UNORM);

    MaterialParameters parameters{
        {"param1", vec2f{0.0f, 1.0f}},
        {"param2", vec4f{0.0f, 1.0f, 2.0f, 3.0f}},
        {"param2", vec3f{0.0f, 1.0f, 2.0f}},  // no effect
        {"param3", vec2f{0.0f, 1.0f}},
        {"texture", texture},
    };

    const auto mat = std::make_shared<Material>(MaterialDesc{.parameters = parameters});

    const auto default_parameters = mat->GetDefaultParameters();
    ASSERT_EQ(default_parameters.size(), 4);

    parameters.erase(std::next(parameters.begin(), 2));

    for (std::size_t i = 0; i < default_parameters.size(); ++i) {
        EXPECT_EQ(default_parameters[i], parameters[i]);
    }
}

TEST(MaterialTest, HasParameter) {
    auto mat = std::make_shared<Material>(MaterialDesc{
        .parameters = {
            {"param1", float{1.0f}},
            {"param2", vec2f{1, 2}},
            {"tex1", std::shared_ptr<Texture>{}},
        }});

    EXPECT_TRUE(mat->HasParameter<float>("param1"));
    EXPECT_TRUE(mat->HasParameter<vec2f>("param2"));
    EXPECT_TRUE(mat->HasParameter<std::shared_ptr<Texture>>("tex1"));
    EXPECT_FALSE(mat->HasParameter<int>("param1"));    // Wrong type
    EXPECT_FALSE(mat->HasParameter<float>("param3"));  // Non-existent parameter
}

TEST(MaterialTest, CreateInstance) {
    const auto         texture = std::make_shared<Texture>(128, 128, hitagi::gfx::Format::R8G8B8A8_UNORM);
    MaterialParameters parameters{
        {"param1", vec2f{0.0f, 1.0f}},
        {"param2", vec4f{0.0f, 1.0f, 2.0f, 3.0f}},
        {"param2", vec3f{0.0f, 1.0f, 2.0f}},  // no effect
        {"param3", vec2f{0.0f, 1.0f}},
        {"texture", texture},
    };

    const MaterialInstance instance{parameters};

    const auto& instance_parameters = instance.GetParameters();

    ASSERT_EQ(instance_parameters.size(), 4);

    parameters.erase(std::next(parameters.begin(), 2));

    for (std::size_t i = 0; i < instance_parameters.size(); ++i) {
        EXPECT_EQ(instance_parameters[i], parameters[i]);
    }
}

TEST(MaterialTest, CreateInstanceFromMaterial) {
    const auto               texture = std::make_shared<Texture>(128, 128, hitagi::gfx::Format::R8G8B8A8_UNORM);
    const MaterialParameters parameters{
        {"param1", vec2f{0.0f, 1.0f}},
        {"param2", vec4f{0.0f, 1.0f, 2.0f, 3.0f}},
        {"param3", vec2f{0.0f, 1.0f}},
        {"texture", texture},
    };

    const auto mat      = std::make_shared<Material>(MaterialDesc{.parameters = parameters});
    const auto instance = mat->CreateInstance();
    ASSERT_TRUE(instance);
    EXPECT_EQ(mat->GetInstances().size(), 1);
    EXPECT_TRUE(mat->GetInstances().contains(instance.get()));

    const auto instance_parameters = instance->GetParameters();
    ASSERT_EQ(instance_parameters.size(), parameters.size());

    for (std::size_t i = 0; i < instance_parameters.size(); ++i) {
        EXPECT_EQ(instance_parameters[i], parameters[i]);
    }
}

TEST(MaterialInstanceTest, GetParameter) {
    const auto         texture = std::make_shared<Texture>(128, 128, hitagi::gfx::Format::R8G8B8A8_UNORM);
    MaterialParameters parameters{
        {"param1", vec2f{0.0f, 1.0f}},
        {"param2", vec4f{0.0f, 1.0f, 2.0f, 3.0f}},
        {"param2", vec3f{0.0f, 1.0f, 2.0f}},  // no effect
        {"param3", vec2f{0.0f, 1.0f}},
        {"texture", texture},
    };
    const MaterialInstance instance{parameters};

    EXPECT_TRUE(instance.GetParameter<vec2f>("param1").has_value());
    EXPECT_TRUE(instance.GetParameter<vec4f>("param2").has_value());
    EXPECT_TRUE(instance.GetParameter<vec2f>("param3").has_value());
    EXPECT_TRUE(instance.GetParameter<std::shared_ptr<Texture>>("texture").has_value());

    EXPECT_FALSE(instance.GetParameter<vec3f>("param1").has_value()) << "Can not get parameter with unmatched type.";
    EXPECT_FALSE(instance.GetParameter<vec3f>("param2").has_value()) << "Can not get override parameter.";
    EXPECT_FALSE(instance.GetParameter<vec4f>("param-no-exists").has_value()) << "Can not get no exists parameter.";
}

TEST(MaterialInstanceTest, InstanceChangeValue) {
    const auto texture  = std::make_shared<Texture>(128, 128, hitagi::gfx::Format::R8G8B8A8_UNORM);
    const auto instance = std::make_shared<MaterialInstance>(MaterialParameters{
        {"param1", float{1.0f}},
        {"param2", vec2f{1, 2}},
        {"tex1", texture},
    });

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
        EXPECT_VEC_EQ(_param2.value(), vec2f(3.0f, 4.0f));
    }

    {
        instance->SetParameter("tex1", std::shared_ptr<Texture>{nullptr});
        auto _tex1 = instance->GetParameter<std::shared_ptr<Texture>>("tex1");
        EXPECT_TRUE(_tex1.has_value());
        EXPECT_EQ(_tex1.value(), nullptr);
    }
}

TEST(MaterialInstanceTest, SetMaterial) {
    MaterialInstance instance(MaterialParameters{
        {"param1", float{3.0f}},
        {"param2", vec3f{1, 2, 3}},
        {"param4", vec4f{1, 2, 3, 4}},
    });

    // no material associated
    {
        EXPECT_EQ(instance.GetMaterial(), nullptr) << "instance should not be associated with any material";
    }

    // associated with mat1
    {
        const auto mat1 = std::make_shared<Material>(MaterialDesc{
            .parameters = {
                {"param1", float{1.0f}},
                {"param2", vec2f{1, 2}},
                {"param3", vec3f{1, 2, 3}},
            }});

        instance.SetMaterial(mat1);

        EXPECT_EQ(instance.GetMaterial(), mat1) << "instance should be associated with mat1";
    }

    // associated with mat2
    {
        const auto mat2 = std::make_shared<Material>(MaterialDesc{
            .parameters = {
                {"param1", float{2.0f}},
                {"param2", vec2f{3, 4}},
                {"param3", vec3f{3, 4, 5}},
            }});

        instance.SetMaterial(mat2);

        EXPECT_EQ(instance.GetMaterial(), mat2) << "instance should be associated with mat2";
    }
}

TEST(MaterialInstanceTest, ParameterLayout) {
    const auto mat = std::make_shared<Material>(MaterialDesc{
        {},
        {},
        {
            //                                                                       offset,  size
            {.name = "param1", .value = vec2f{1, 2}},                        //       0,     8
            {.name = "param2", .value = float{1}},                           //       8,     4
            {.name = "param3", .value = vec4f{1, 2, 3, 4}},                  //      12,    16
            {.name = "param4", .value = std::shared_ptr<Texture>{nullptr}},  // no effect
            {.name = "param5", .value = vec2f{1, 2}},                        //      28,     8
            {.name = "param6", .value = vec3f{1, 2, 3}},                     //      36,    12
        }});

    const auto instance = mat->CreateInstance();
    ASSERT_TRUE(instance);

    const auto buffer = instance->GenerateMaterialBuffer();
    ASSERT_EQ(buffer.GetDataSize(), 48);

    EXPECT_VEC_EQ(*reinterpret_cast<const vec2f*>(buffer.GetData() + 0), vec2f(1, 2));
    EXPECT_EQ(*reinterpret_cast<const float*>(buffer.GetData() + 8), 1.0f);
    EXPECT_VEC_EQ(*reinterpret_cast<const vec4f*>(buffer.GetData() + 12), vec4f(1, 2, 3, 4));
    EXPECT_VEC_EQ(*reinterpret_cast<const vec2f*>(buffer.GetData() + 28), vec2f(1, 2));
    EXPECT_VEC_EQ(*reinterpret_cast<const vec3f*>(buffer.GetData() + 36), vec3f(1, 2, 3));
}

TEST(MaterialInstanceTest, CloneMaterialInstance) {
    const auto mat = std::make_shared<Material>(MaterialDesc{
        .parameters = {
            {"param1", float{1.0f}},
        }});

    const auto instance1 = mat->CreateInstance();
    ASSERT_TRUE(instance1);

    instance1->SetParameter("param1", 2.0f);
    MaterialInstance instance2 = *instance1;  // Use copy constructor

    EXPECT_EQ(mat->GetInstances().size(), 2);
    EXPECT_TRUE(mat->GetInstances().contains(instance1.get()));

    EXPECT_EQ(instance1->GetParameter<float>("param1").value(), 2.0f);
    EXPECT_EQ(instance2.GetParameter<float>("param1").value(), 2.0f);
}

TEST(MaterialInstanceTest, GetSplitParametersWithoutMaterial) {
    const MaterialInstance instance{
        {
            {"instance_param", vec3f{3, 4, 5}},
        },
    };

    const auto split_params = instance.GetSplitParameters();
    EXPECT_EQ(split_params.only_in_instance.size(), 1);
    EXPECT_EQ(split_params.only_in_instance[0].name, "instance_param");
    EXPECT_TRUE(split_params.only_in_material.empty());
    EXPECT_TRUE(split_params.in_both.empty());
}

TEST(MaterialInstanceTest, GetSplitParametersNoOverlap) {
    const auto mat = std::make_shared<Material>(MaterialDesc{
        .parameters = {
            {"param1", vec2f{1, 2}},
            {"param2", vec3f{3, 4, 5}},
        }});

    MaterialInstance instance{
        {
            {"param1", vec3f{3, 4, 5}},
            {"param3", vec4f{6, 7, 8, 9}},
        },
    };
    instance.SetMaterial(mat);

    const auto split_params = instance.GetSplitParameters();

    ASSERT_EQ(split_params.only_in_instance.size(), 2);
    EXPECT_EQ(split_params.only_in_instance[0].name, "param1");
    EXPECT_EQ(split_params.only_in_instance[1].name, "param3");
    EXPECT_VEC_EQ(std::get<vec3f>(split_params.only_in_instance[0].value), vec3f(3, 4, 5));
    EXPECT_VEC_EQ(std::get<vec4f>(split_params.only_in_instance[1].value), vec4f(6, 7, 8, 9));

    ASSERT_EQ(split_params.only_in_material.size(), 2);
    EXPECT_EQ(split_params.only_in_material[0].name, "param1");
    EXPECT_EQ(split_params.only_in_material[1].name, "param2");
    EXPECT_VEC_EQ(std::get<vec2f>(split_params.only_in_material[0].value), vec2f(1, 2));
    EXPECT_VEC_EQ(std::get<vec3f>(split_params.only_in_material[1].value), vec3f(3, 4, 5));

    EXPECT_TRUE(split_params.in_both.empty());
}

TEST(MaterialInstanceTest, GetSplitParametersWithOverlap) {
    const auto mat = std::make_shared<Material>(MaterialDesc{
        .parameters = {
            {"param1", vec2f{1, 2}},
            {"param2", vec3f{3, 4, 5}},
        }});

    MaterialInstance instance{
        {
            {"param1", vec2f{3, 4}},
            {"param3", vec4f{6, 7, 8, 9}},
        },
    };
    instance.SetMaterial(mat);

    const auto split_params = instance.GetSplitParameters();

    ASSERT_EQ(split_params.only_in_instance.size(), 1);
    EXPECT_EQ(split_params.only_in_instance[0].name, "param3");
    EXPECT_VEC_EQ(std::get<vec4f>(split_params.only_in_instance[0].value), vec4f(6, 7, 8, 9));

    ASSERT_EQ(split_params.only_in_material.size(), 1);
    EXPECT_EQ(split_params.only_in_material[0].name, "param2");
    EXPECT_VEC_EQ(std::get<vec3f>(split_params.only_in_material[0].value), vec3f(3, 4, 5));

    ASSERT_EQ(split_params.in_both.size(), 1);
    EXPECT_EQ(split_params.in_both[0].name, "param1");
    EXPECT_VEC_EQ(std::get<vec2f>(split_params.in_both[0].value), vec2f(3, 4));
}

TEST(MaterialInstanceTest, AssociatedTextures) {
    const auto tex1 = std::make_shared<Texture>(128, 128, hitagi::gfx::Format::R8G8B8A8_UNORM);
    const auto tex2 = std::make_shared<Texture>(128, 128, hitagi::gfx::Format::R8G8B8A8_UNORM);
    const auto tex3 = std::make_shared<Texture>(128, 128, hitagi::gfx::Format::R8G8B8A8_UNORM);
    const auto mat  = std::make_shared<Material>(MaterialDesc{
         .parameters = {
            {"tex1", tex1},
            {"tex2", tex2},
        }});

    MaterialInstance instance{
        {
            {"tex1", tex1},
            {"tex3", tex3},
        },
    };
    instance.SetMaterial(mat);

    const auto textures = instance.GetAssociatedTextures();

    EXPECT_EQ(textures.size(), 2);
    EXPECT_EQ(textures[0], tex1);
    EXPECT_EQ(textures[1], tex2);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}