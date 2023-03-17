#include <hitagi/gfx/gpu_resource.hpp>
#include <hitagi/utils/hash.hpp>
#include <hitagi/utils/test.hpp>

using namespace hitagi;
using namespace hitagi::gfx;

TEST(GfxTest, DescHashTest) {
    {
        GPUBuffer::Desc
            buffer_desc_1 = {},
            buffer_desc_2 = {
                .name          = "buffer_desc_1",
                .element_size  = 16,
                .element_count = 32,
                .usages        = GPUBufferUsageFlags::Vertex,
            };

        buffer_desc_2.name = "buffer_desc_2";
        EXPECT_NE(buffer_desc_1, buffer_desc_2);
        EXPECT_NE(utils::hash(buffer_desc_1), utils::hash(buffer_desc_2));

        buffer_desc_2 = buffer_desc_1;
        EXPECT_EQ(buffer_desc_1, buffer_desc_2);
        EXPECT_EQ(utils::hash(buffer_desc_1), utils::hash(buffer_desc_2));
    }

    {
        Texture::Desc
            tex_desc_1 = {},
            tex_desc_2 = {
                .name         = "tex_desc_1",
                .width        = 32,
                .height       = 32,
                .depth        = 1,
                .array_size   = 1,
                .format       = Format::R8G8B8A8_UNORM,
                .mip_levels   = 1,
                .sample_count = 1,
                .is_cube      = false,
                .clear_value  = {
                     .color = math::vec4f{1, 0, 0, 1},
                },
                .usages = TextureUsageFlags::RTV,
            };
        EXPECT_NE(tex_desc_1, tex_desc_2);
        EXPECT_NE(utils::hash(tex_desc_1), utils::hash(tex_desc_2));

        tex_desc_2 = tex_desc_1;
        EXPECT_EQ(tex_desc_1, tex_desc_2);
        EXPECT_EQ(utils::hash(tex_desc_1), utils::hash(tex_desc_2));
    }

    {
        Sampler::Desc sampler_desc_1 = {},
                      sampler_desc_2 = {
                          .name = "sampler_desc_1",
                      };

        EXPECT_NE(sampler_desc_1, sampler_desc_2);
        EXPECT_NE(utils::hash(sampler_desc_1), utils::hash(sampler_desc_2));

        sampler_desc_2 = sampler_desc_1;
        EXPECT_EQ(sampler_desc_1, sampler_desc_2);
        EXPECT_EQ(utils::hash(sampler_desc_1), utils::hash(sampler_desc_2));
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}