// this bindless header inspired by
// https://medium.com/traverse-research/bindless-rendering-templates-8920ca41326f
namespace hitagi {
#define NUM_STATIC_SAMPLERS 4

    struct BindlessHandle {
        uint index;
        uint read_index() {
            return index;
        }
#ifdef __spirv__
        uint write_index() {
            return read_index();
        }
#else
        // On D3D12 side , since SRV and UAV are separte descriptors,
        // we use descriptor pair on descriptor heap to solve this problem
        uint write_index() {
            return read_index() + 1;
        }
#endif
    };

    // this is the push constant value, we will use it to read bindless handles
    // which refers to actual resource at descriptor heap
    struct BindingsOffset {
        BindlessHandle binding_offset;
        uint           user_data_0;
        uint           user_data_1;
        uint           user_data_2;
    };

    // --------- Binding Area Start ---------
#ifdef __spirv__
    // Since a push constant in DirectX 12 must register to (bx, spacey)
    // that will be bound to a descriptor set binding on the SPIR-V side.
    // We must use macro to handle this problem.
    [[vk::push_constant]]
    ConstantBuffer<BindingsOffset> g_bindings_offset;

    // Since SPIR-V dose not have `ResourceDescriptorHeap` concept, we simulate it here
    // ! struct buffer and constant buffer need type qualifier advancing, so we have to
    // ! use StorageBuffer(a.k.a ByteAddressBuffer) to simulate those buffer, which may have performence issue.
    // ! When we use `ResourceDescriptorHeap` on D3D12 side, it is striaightforward.
    [[vk::binding(0, 0)]]
    ByteAddressBuffer g_byte_address_buffer[];
    [[vk::binding(0, 0)]]
    RWByteAddressBuffer g_rw_byte_address_buffer[];

#define DEFINE_TEXTURE_BINDING(TextureType, binding_index, set_index) \
    [[vk::binding(binding_index, binding_index)]]                     \
    TextureType<float> g_##TextureType##_float[];                     \
    [[vk::binding(binding_index, binding_index)]]                     \
    TextureType<float2> g_##TextureType##_float2[];                   \
    [[vk::binding(binding_index, binding_index)]]                     \
    TextureType<float3> g_##TextureType##_float3[];                   \
    [[vk::binding(binding_index, binding_index)]]                     \
    TextureType<float4> g_##TextureType##_float4[];

    [[vk::binding(0, 1)]]
    SamplerState static_samplers[NUM_STATIC_SAMPLERS];
    // Support Texture2D now
    DEFINE_TEXTURE_BINDING(Texture1D, NUM_STATIC_SAMPLERS, 1)
    DEFINE_TEXTURE_BINDING(Texture2D, NUM_STATIC_SAMPLERS, 1)
    DEFINE_TEXTURE_BINDING(Texture3D, NUM_STATIC_SAMPLERS, 1)
    DEFINE_TEXTURE_BINDING(TextureCube, NUM_STATIC_SAMPLERS, 1)

    DEFINE_TEXTURE_BINDING(RWTexture1D, 0, 2)
    DEFINE_TEXTURE_BINDING(RWTexture2D, 0, 2)
    DEFINE_TEXTURE_BINDING(RWTexture3D, 0, 2)

    // --------- DescriptorHeap Helper Arrea Begin -------------

    template <typename ResourceType>
    struct HandleWrapper {
        BindlessHandle _internal;
    };

    struct SimpleBuffer;

    struct VkDescriptorHeapHelper {
        ByteAddressBuffer operator[](HandleWrapper<SimpleBuffer> handle) {
            return g_byte_address_buffer[handle._internal.index];
        }

#define DEFINE_TEXTURE_GET_FN(TextureType, ValueType)                                          \
    TextureType<ValueType> operator[](HandleWrapper<TextureType<ValueType> > handle) {         \
        TextureType<ValueType> result = g_##TextureType##_##ValueType[handle._internal.index]; \
        return result;                                                                         \
    }

#define DEFINE_TEXTURE_GET_FN_WITH_VALUE(TextureType) \
    DEFINE_TEXTURE_GET_FN(TextureType, float)         \
    DEFINE_TEXTURE_GET_FN(TextureType, float2)        \
    DEFINE_TEXTURE_GET_FN(TextureType, float3)        \
    DEFINE_TEXTURE_GET_FN(TextureType, float4)

        DEFINE_TEXTURE_GET_FN_WITH_VALUE(Texture1D)
        DEFINE_TEXTURE_GET_FN_WITH_VALUE(Texture2D)
        DEFINE_TEXTURE_GET_FN_WITH_VALUE(Texture3D)
        DEFINE_TEXTURE_GET_FN_WITH_VALUE(TextureCube)

        DEFINE_TEXTURE_GET_FN_WITH_VALUE(RWTexture1D)
        DEFINE_TEXTURE_GET_FN_WITH_VALUE(RWTexture2D)
        DEFINE_TEXTURE_GET_FN_WITH_VALUE(RWTexture3D)
    };

    static VkDescriptorHeapHelper VkResourceDescriptorHeap;

#define DescriptorHeap(ResourceType, handle) VkResourceDescriptorHeap[(HandleWrapper<ResourceType>)handle]
    // --------- DescriptorHeap Helper Arrea End ---------------

    template <typename T>
    T load_bindless() {
        T result = DescriptorHeap(SimpleBuffer, g_bindings_offset.binding_offset.read_index()).Load<T>(0);
        return result;
    }

#else
    // Note that we use space 16 here to accept our push constant on D3D12 side.
    ConstantBuffer<BindingsOffset> g_bindings_offset : register(b0, space16);

    template <typename T>
    T load_bindless() {
        ConstantBuffer<T> result = ResourceDescriptorHeap[g_bindings_offset.binding_offset.read_index()];
        return result;
    }

#define DescriptorHeap(ResourceType, handle) ResourceDescriptorHeap[handle.index]
#endif

    // --------- Binding Area End  ---------

    struct SimpleBuffer {
        BindlessHandle handle;

        template <typename T>
        T load() {
#ifdef __spirv__
            T result = DescriptorHeap(SimpleBuffer, handle).Load<T>(0);
#else
            ConstantBuffer<T> result = DescriptorHeap(SimpleBuffer, handle);
#endif
            return result;
        }
    };

    struct Texture {
        BindlessHandle handle;

#if __SHADER_TARGET_STAGE == __SHADER_STAGE_PIXEL  // sampling with implicit lod is only allowed in fragment shaders
        template <typename T>
        T sample(SamplerState sampler, float2 uv) {
            Texture2D<T> texture = DescriptorHeap(Texture2D<T>, handle);
            return texture.Sample(sampler, uv);
        }
#endif

        template <typename T>
        T sample_level(SamplerState sampler, float2 uv, float mip) {
            Texture2D<T> texture = DescriptorHeap(Texture2D<T>, handle);
            return texture.SampleLevel(sampler, uv, mip);
        }
    };

}

