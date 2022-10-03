#include <hitagi/graphics/render_graph.hpp>
#include <hitagi/utils/test.hpp>

using namespace hitagi::resource;
using namespace hitagi::gfx;
using namespace hitagi::utils;
using namespace hitagi::math;
using namespace testing;

class MockDevice : public DeviceAPI {
public:
    MockDevice() : DeviceAPI(APIType::Mock) {}

    MOCK_METHOD(void, CreateSwapChain, (std::uint32_t width, std::uint32_t height, unsigned frame_count, Format format, void* window), (final));
    MOCK_METHOD(std::size_t, ResizeSwapChain, (std::uint32_t width, std::uint32_t height), (final));
    MOCK_METHOD(void, Present, (), (final));
    MOCK_METHOD(void, InitRenderTargetFromSwapChain, (Texture & rt, std::size_t frame_index), (final));
    MOCK_METHOD(void, InitVertexBuffer, (VertexArray & vb), (final));
    MOCK_METHOD(void, InitIndexBuffer, (IndexArray & ib), (final));
    MOCK_METHOD(void, InitTexture, (Texture & tb), (final));
    MOCK_METHOD(void, InitConstantBuffer, (ConstantBuffer & cb), (final));
    MOCK_METHOD(void, InitSampler, (Sampler & sampler), (final));
    MOCK_METHOD(void, InitPipelineState, (PipelineState & pipeline), (final));
    MOCK_METHOD((std::shared_ptr<IGraphicsCommandContext>), CreateGraphicsCommandContext, (std::string_view name), (final));

    void RetireResource(std::unique_ptr<backend::Resource> resource) final {
        RetireResourceMock(resource.get());
    }
    MOCK_METHOD(void, RetireResourceMock, (backend::Resource * resource));
    MOCK_METHOD(bool, IsFenceComplete, (std::uint64_t fence_value), (final));
    MOCK_METHOD(void, WaitFence, (std::uint64_t fence_value), (final));
    MOCK_METHOD(void, IdleGPU, (), (final));
};

class MockCommandContext : public IGraphicsCommandContext {
public:
    MOCK_METHOD(void, ClearRenderTarget, (const Texture& rt), (final));
    MOCK_METHOD(void, ClearDepthBuffer, (const Texture& depth_buffer), (final));

    MOCK_METHOD(void, SetRenderTarget, (const Texture& rt), (final));
    MOCK_METHOD(void, SetRenderTargetAndDepthBuffer, (const Texture& rt, const Texture& depth_buffer), (final));
    MOCK_METHOD(void, SetPipelineState, (const PipelineState& pipeline), (final));

    MOCK_METHOD(void, SetViewPort, (std::uint32_t x, std::uint32_t y, std::uint32_t width, std::uint32_t height), (final));
    MOCK_METHOD(void, SetScissorRect, (std::uint32_t left, std::uint32_t top, std::uint32_t right, std::uint32_t bottom), (final));
    MOCK_METHOD(void, SetViewPortAndScissor, (std::uint32_t x, std::uint32_t y, std::uint32_t width, std::uint32_t height), (final));
    MOCK_METHOD(void, SetBlendFactor, (vec4f color), (final));

    // Resource Binding
    MOCK_METHOD(void, BindResource, (std::uint32_t slot, const ConstantBuffer& cb, std::size_t index), (final));
    MOCK_METHOD(void, BindResource, (std::uint32_t slot, const Texture& texture), (final));
    MOCK_METHOD(void, BindResource, (std::uint32_t slot, const Sampler& sampler), (final));
    MOCK_METHOD(void, Set32BitsConstants, (std::uint32_t slot, const std::uint32_t* data, std::size_t count), (final));
    MOCK_METHOD(void, BindDynamicStructuredBuffer, (std::uint32_t slot, const std::byte* data, std::size_t size), (final));
    MOCK_METHOD(void, BindDynamicConstantBuffer, (std::uint32_t slot, const std::byte* data, std::size_t size), (final));

    // when dirty is set, it will be upload to gpu, (reallocated if buffer has been expanded)
    MOCK_METHOD(void, BindMeshBuffer, (const Mesh& mesh), (final));
    MOCK_METHOD(void, BindVertexBuffer, (const VertexArray& vertices), (final));
    MOCK_METHOD(void, BindIndexBuffer, (const IndexArray& indices), (final));

    // Upload cpu buffer directly
    MOCK_METHOD(void, BindDynamicVertexBuffer, (const VertexArray& vertices), (final));
    MOCK_METHOD(void, BindDynamicIndexBuffer, (const IndexArray& indices), (final));

    MOCK_METHOD(void, UpdateBuffer, (backend::Resource * resource, std::size_t offset, const std::byte* data, std::size_t data_size), (final));
    MOCK_METHOD(void, UpdateVertexBuffer, (VertexArray & vertices), (final));
    MOCK_METHOD(void, UpdateIndexBuffer, (IndexArray & indices), (final));
    // src_box = {left, top, front, right, bootom, back}
    // dest_point = {x, y, z}
    MOCK_METHOD(void, CopyTextureRegion, (Texture & src, (std::array<std::uint32_t, 6>)src_box, Texture& dest, (std::array<std::uint32_t, 3>)dest_point), (final));

    MOCK_METHOD(void, Draw, (std::size_t vertex_count, std::size_t vertex_start_offset), (final));
    MOCK_METHOD(void, DrawIndexed, (std::size_t index_count, std::size_t start_index_location, std::size_t base_vertex_location), (final));
    MOCK_METHOD(void, DrawInstanced, (std::size_t vertex_count, std::size_t instance_count, std::size_t start_vertex_location, std::size_t start_instance_location), (final));
    MOCK_METHOD(void, DrawIndexedInstanced, (std::size_t index_count, std::size_t instance_count, std::size_t start_index_location, std::size_t base_vertex_location, std::size_t start_instance_location), (final));

    MOCK_METHOD(void, Present, (const Texture& rt), (final));

    MOCK_METHOD(std::uint64_t, Finish, (bool wait_for_complete), (final));
    MOCK_METHOD(void, Reset, (), (final));
};

class RenderGraphTest : public ::testing::Test {
public:
    RenderGraphTest()
        : device(std::make_shared<MockDevice>()),
          context(std::make_shared<MockCommandContext>()) {}

    std::shared_ptr<MockDevice>         device;
    std::shared_ptr<MockCommandContext> context;
};

TEST_F(RenderGraphTest, GraphBuild) {
    RenderGraph render_graph(*device);

    struct ColorPass {
        ResourceHandle texture;
        ResourceHandle output;
    };

    auto color_pass = render_graph.AddPass<ColorPass>(
        "Color",
        [](RenderGraph::Builder& builder, ColorPass& data) {
            data.texture = builder.Create(
                "texture",
                Texture{
                    .bind_flags = Texture::BindFlag::ShaderResource,
                    .format     = Format::R8G8B8A8_UNORM,
                    .width      = 100,
                    .height     = 100,
                });
            data.output = builder.Write(data.texture);
        },
        [](const RenderGraph::ResourceHelper& helper, const ColorPass& data, IGraphicsCommandContext* context) {
            EXPECT_EQ(data.output, 1);
        });

    EXPECT_EQ(color_pass.output, 1);
    EXPECT_CALL(*device, InitTexture(_))
        .Times(1)
        .WillOnce(testing::WithArg<0>([](Texture& tex) {
            tex.gpu_resource = std::make_unique<backend::Resource>();
        }));
    EXPECT_CALL(*device, RetireResourceMock(Field(&backend::Resource::fence_value, 1)))
        .Times(1);
    EXPECT_CALL(*device, CreateGraphicsCommandContext(_)).WillRepeatedly(Return(context));
    EXPECT_CALL(*context, Present(_));

    EXPECT_TRUE(render_graph.Compile());
    render_graph.Execute();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}