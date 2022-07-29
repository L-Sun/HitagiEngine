#include <hitagi/core/memory_manager.hpp>
#include <hitagi/resource/asset_manager.hpp>

#include <hitagi/utils/test.hpp>

using namespace hitagi;
using namespace hitagi::resource;
using namespace hitagi::math;
using namespace hitagi::testing;

TEST(ImageParserTest, ErrorPath) {
    auto image = asset_manager->ImportImage("assets/test/azcxxcacas.jpg");
    EXPECT_TRUE(image == nullptr);
}

TEST(ImageParserTest, Jpeg) {
    auto image = asset_manager->ImportImage("assets/test/test.jpg");
    EXPECT_TRUE(image != nullptr);
    EXPECT_EQ(image->Width(), 278);
    EXPECT_EQ(image->Height(), 152);
}

TEST(ImageParserTest, Tga) {
    auto image = asset_manager->ImportImage("assets/test/test.tga");
    EXPECT_TRUE(image != nullptr);
    EXPECT_EQ(image->Width(), 278);
    EXPECT_EQ(image->Height(), 152);
}

TEST(ImageParserTest, Png) {
    auto image = asset_manager->ImportImage("assets/test/test.png");
    EXPECT_TRUE(image != nullptr);
    EXPECT_EQ(image->Width(), 278);
    EXPECT_EQ(image->Height(), 152);
}

TEST(ImageParserTest, Bmp) {
    auto image = asset_manager->ImportImage("assets/test/test.bmp");
    EXPECT_TRUE(image != nullptr);
    EXPECT_EQ(image->Width(), 278);
    EXPECT_EQ(image->Height(), 152);
}

TEST(SceneParserTest, Fbx) {
    Scene scene;
    asset_manager->ImportScene(scene, "assets/test/test.fbx");
    EXPECT_EQ(scene.cameras.size(), 1);
    EXPECT_EQ(scene.geometries.size(), 1);
    EXPECT_EQ(scene.lights.size(), 1);
}

TEST(MaterialParserTest, Inner) {
    auto instance = asset_manager->ImportMaterial("assets/test/test-mat.json");
    EXPECT_TRUE(instance != nullptr);

    auto mat = instance->GetMaterial().lock();
    EXPECT_TRUE(mat != nullptr);
    EXPECT_EQ(mat->GetNumInstances(), 1);
    EXPECT_EQ(mat->GetPrimitiveType(), PrimitiveType::TriangleList);

    EXPECT_TRUE(instance->GetValue<vec4f>("diffuse").has_value());
    EXPECT_TRUE(instance->GetValue<vec4f>("ambient").has_value());
    EXPECT_TRUE(instance->GetValue<vec4f>("specular").has_value());
    EXPECT_TRUE(instance->GetValue<float>("specular_power").has_value());

    vector_eq(instance->GetValue<vec4f>("diffuse").value(), vec4f(1.0f, 0.8f, 0.5f, 1.0f));
    vector_eq(instance->GetValue<vec4f>("ambient").value(), vec4f(0.5f, 0.8f, 0.5f, 1.0f));
    vector_eq(instance->GetValue<vec4f>("specular").value(), vec4f(0.0f, 0.8f, 0.5f, 1.0f));
    EXPECT_DOUBLE_EQ(instance->GetValue<float>("specular_power").value(), 23.0);
}

int main(int argc, char* argv[]) {
    spdlog::set_level(spdlog::level::debug);
    auto file_io_manager = std::make_unique<core::FileIOManager>();
    auto asset_manager   = std::make_unique<AssetManager>();

    hitagi::file_io_manager = file_io_manager.get();
    hitagi::asset_manager   = asset_manager.get();

    file_io_manager->Initialize();
    asset_manager->Initialize();

    ::testing::InitGoogleTest(&argc, argv);
    int test_result = RUN_ALL_TESTS();

    asset_manager->Finalize();
    file_io_manager->Finalize();

    return test_result;
}
