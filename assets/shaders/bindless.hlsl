// this bindless header inspired by
// https://medium.com/traverse-research/bindless-rendering-templates-8920ca41326f

namespace hitagi {

    struct BindlessHandle {
        uint index;
        uint type;
        uint writeable;
        uint version;
    };

    struct BindlessMetaInfo {
        BindlessHandle handle;
    };

    template <typename T>
    bool valid(T resource) {
        return resource.handle.type != 3;
    }

#ifdef __spirv__
    [[vk::push_constant]]
    ConstantBuffer<BindlessMetaInfo> g_bindless_meta_info;

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

    template <typename T>
    T load_bindless() {
        T result = g_byte_address_buffer[g_bindless_meta_info.handle.index].Load<T>(0);
        return result;
    }

    struct SimpleBuffer {
        BindlessHandle handle;

        template <typename T>
        T load() {
            return g_byte_address_buffer[handle.index].Load<T>(0);
        }
    };

    struct RWSimpleBuffer {
        BindlessHandle handle;

        template <typename T>
        T load() {
            return g_rw_byte_address_buffer[handle.index].Load<T>(0);
        }
    };

    template <typename TextureValueType>
    struct HandleWrapper {
        BindlessHandle _internal;
    };

    struct TextureHelper {
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
    static TextureHelper g_texture_helper;

#define TEXTURE_GET_FN(TextureValueType, handle) g_texture_helper[(HandleWrapper<TextureValueType>)handle]

    struct Texture {
        BindlessHandle handle;

        template <typename T>
        T load(uint pos) {
            Texture1D<T> texture = TEXTURE_GET_FN(Texture1D<T>, handle);
            return texture.Load(pos);
        }

        template <typename T>
        T load(uint2 pos) {
            Texture2D<T> texture = TEXTURE_GET_FN(Texture2D<T>, handle);
            return texture.Load(uint3(pos, 0));
        }

        template <typename T>
        T sample(SamplerState sampler, float2 uv) {
            Texture2D<T> texture = TEXTURE_GET_FN(Texture2D<T>, handle);
            return texture.Sample(sampler, uv);
        }

        template <typename T>
        T sample_level(SamplerState sampler, float2 uv, float mip) {
            Texture2D<T> texture = TEXTURE_GET_FN(Texture2D<T>, handle);
            return texture.SampleLevel(sampler, uv, mip);
        }
    };

    struct Sampler {
        BindlessHandle handle;

        SamplerState load() {
            return g_samplers[handle.index];
        }
    };

#else
    ConstantBuffer<BindlessMetaInfo> g_bindless_meta_info : register(b0, space0);

    template <typename T>
    T load_bindless() {
        ConstantBuffer<T> result = ResourceDescriptorHeap[NonUniformResourceIndex(g_bindless_meta_info.handle.index)];
        return result;
    }

    struct SimpleBuffer {
        BindlessHandle handle;

        template <typename T>
        T load() {
            ConstantBuffer<T> result = ResourceDescriptorHeap[NonUniformResourceIndex(handle.index)];
            return result;
        }
    };

    struct Texture {
        BindlessHandle handle;

        template <typename T>
        T load(uint pos) {
            Texture1D<T> texture = ResourceDescriptorHeap[NonUniformResourceIndex(handle.index)];
            return texture.Load(pos);
        }

        template <typename T>
        T load(uint2 pos) {
            Texture2D<T> texture = ResourceDescriptorHeap[NonUniformResourceIndex(handle.index)];
            return texture.Load(uint3(pos, 0));
        }

        template <typename T>
        T sample(SamplerState sampler, float2 uv) {
            Texture2D<T> texture = ResourceDescriptorHeap[NonUniformResourceIndex(handle.index)];
            return texture.Sample(sampler, uv);
        }

        template <typename T>
        T sample_level(SamplerState sampler, float2 uv, float mip) {
            Texture2D<T> texture = ResourceDescriptorHeap[NonUniformResourceIndex(handle.index)];
            return texture.SampleLevel(sampler, uv, mip);
        }
    };

    struct Sampler {
        BindlessHandle handle;

        SamplerState load() {
            SamplerState sampler = SamplerDescriptorHeap[NonUniformResourceIndex(handle.index)];
            return sampler;
        }
    };
#endif
}
