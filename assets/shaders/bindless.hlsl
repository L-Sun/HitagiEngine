// this bindless header inspired by
// https://medium.com/traverse-research/bindless-rendering-templates-8920ca41326f
namespace hitagi {
    struct BindlessHandle {
        uint index : 32;
        uint version : 16;
        uint type : 8;
        uint tag : 8;  // 255 is invalid handle

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
    [[vk::binding(binding_index, set_index)]]                         \
    TextureType<float> g_##TextureType##_float[];                     \
    [[vk::binding(binding_index, set_index)]]                         \
    TextureType<float2> g_##TextureType##_float2[];                   \
    [[vk::binding(binding_index, set_index)]]                         \
    TextureType<float3> g_##TextureType##_float3[];                   \
    [[vk::binding(binding_index, set_index)]]                         \
    TextureType<float4> g_##TextureType##_float4[];

    // Support Texture2D now
    DEFINE_TEXTURE_BINDING(Texture1D, 0, 1)
    DEFINE_TEXTURE_BINDING(Texture2D, 0, 1)
    DEFINE_TEXTURE_BINDING(Texture3D, 0, 1)
    DEFINE_TEXTURE_BINDING(TextureCube, 0, 1)

    DEFINE_TEXTURE_BINDING(RWTexture1D, 0, 2)
    DEFINE_TEXTURE_BINDING(RWTexture2D, 0, 2)
    DEFINE_TEXTURE_BINDING(RWTexture3D, 0, 2)

    [[vk::binding(0, 3)]]
    SamplerState g_samplers[];

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

#define _ResourceDescriptorHeap(ResourceType, handle) VkResourceDescriptorHeap[(HandleWrapper<ResourceType>)handle]
#define _SamplerDescriptorHeap(handle) g_samplers[handle.index]
    // --------- DescriptorHeap Helper Arrea End ---------------

    template <typename T>
    T load_bindless() {
        T result = _ResourceDescriptorHeap(SimpleBuffer, g_bindings_offset.binding_offset.read_index()).Load<T>(0);
        return result;
    }

#else
    ConstantBuffer<BindingsOffset> g_bindings_offset : register(b0);

    template <typename T>
    T load_bindless() {
        ConstantBuffer<T> result = ResourceDescriptorHeap[g_bindings_offset.binding_offset.read_index()];
        return result;
    }

#define _ResourceDescriptorHeap(ResourceType, handle) ResourceDescriptorHeap[handle.index]
#define _SamplerDescriptorHeap(handle) SamplerDescriptorHeap[handle.index]
#endif

    // --------- Binding Area End  ---------

    struct SimpleBuffer {
        BindlessHandle handle;

        bool valid() {
            return handle.tag != 255;
        }

        template <typename T>
        T load() {
#ifdef __spirv__
            T result = _ResourceDescriptorHeap(SimpleBuffer, handle).Load<T>(0);
#else
            ConstantBuffer<T> result = _ResourceDescriptorHeap(SimpleBuffer, handle);
#endif
            return result;
        }
    };

    struct Texture {
        BindlessHandle handle;

        bool valid() {
            return handle.tag != 255;
        }

        template <typename T>
        T load(uint pos) {
            Texture1D<T> texture = _ResourceDescriptorHeap(Texture1D<T>, handle);
            return texture.Load(pos);
        }

        template <typename T>
        T load(uint2 pos) {
            Texture2D<T> texture = _ResourceDescriptorHeap(Texture2D<T>, handle);
            return texture.Load(uint3(pos, 0));
        }

        template <typename T>
        T sample(SamplerState sampler, float2 uv) {
            Texture2D<T> texture = _ResourceDescriptorHeap(Texture2D<T>, handle);
            return texture.Sample(sampler, uv);
        }

        template <typename T>
        T sample_level(SamplerState sampler, float2 uv, float mip) {
            Texture2D<T> texture = _ResourceDescriptorHeap(Texture2D<T>, handle);
            return texture.SampleLevel(sampler, uv, mip);
        }
    };

    struct Sampler {
        BindlessHandle handle;

        bool valid() {
            return handle.tag != 255;
        }

        SamplerState load() {
            return _SamplerDescriptorHeap(handle);
        }
    };
}

