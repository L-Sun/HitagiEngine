// Use Slang's typed descriptor handles. The backend maps the standard
// non-mutable binding numbers to its descriptor-heap regions.
export T getDescriptorFromHandle<T : IOpaqueDescriptor>(DescriptorHandle<T> handle)
{
    return defaultGetDescriptorFromHandle(handle, BindlessDescriptorOptions.None);
}

namespace hitagi {
    struct BindlessHandle {
        uint index;
        uint type;
        uint writeable;
        uint version;
    };

    struct BindlessMetaInfo {
        BindlessHandle handle;
        uint record_index;
        uint record_stride;
    };

    bool valid(BindlessHandle handle) { return handle.type != 3; }

#ifndef HITAGI_SPIRV
#define HITAGI_SPIRV 0
#endif
#if HITAGI_SPIRV
    [[vk::push_constant]]
    ConstantBuffer<BindlessMetaInfo> g_bindless_meta_info;
#else
    ConstantBuffer<BindlessMetaInfo> g_bindless_meta_info : register(b0, space0);
#endif

    T load_bindless<T>() {
        DescriptorHandle<ByteAddressBuffer> buffer = DescriptorHandle<ByteAddressBuffer>(uint2(g_bindless_meta_info.handle.index, 0));
        return nonuniform(buffer).Load<T>(g_bindless_meta_info.record_index * g_bindless_meta_info.record_stride);
    }

    struct SimpleBuffer {
        BindlessHandle handle;
        T load<T>(uint byte_offset = 0) {
            DescriptorHandle<ByteAddressBuffer> buffer = DescriptorHandle<ByteAddressBuffer>(uint2(handle.index, 0));
            return nonuniform(buffer).Load<T>(byte_offset);
        }
    };

    struct RWSimpleBuffer {
        BindlessHandle handle;
        T load<T>(uint byte_offset = 0) {
            DescriptorHandle<RWByteAddressBuffer> buffer = DescriptorHandle<RWByteAddressBuffer>(uint2(handle.index, 0));
            return nonuniform(buffer).Load<T>(byte_offset);
        }
        void store<T>(T value, uint byte_offset = 0) {
            DescriptorHandle<RWByteAddressBuffer> buffer = DescriptorHandle<RWByteAddressBuffer>(uint2(handle.index, 0));
            nonuniform(buffer).Store<T>(byte_offset, value);
        }
    };

    struct Texture {
        BindlessHandle handle;
        T load<T : ITexelElement>(uint pos) {
            DescriptorHandle<Texture1D<T>> texture = DescriptorHandle<Texture1D<T>>(uint2(handle.index, 0));
            return nonuniform(texture)->Load(int2(pos, 0));
        }
        T load<T : ITexelElement>(uint2 pos) {
            DescriptorHandle<Texture2D<T>> texture = DescriptorHandle<Texture2D<T>>(uint2(handle.index, 0));
            return nonuniform(texture)->Load(int3(pos, 0));
        }
        T sample<T : ITexelElement>(SamplerState sampler, float2 uv) {
            DescriptorHandle<Texture2D<T>> texture = DescriptorHandle<Texture2D<T>>(uint2(handle.index, 0));
            return nonuniform(texture)->Sample(sampler, uv);
        }
        T sample_level<T : ITexelElement>(SamplerState sampler, float2 uv, float mip) {
            DescriptorHandle<Texture2D<T>> texture = DescriptorHandle<Texture2D<T>>(uint2(handle.index, 0));
            return nonuniform(texture)->SampleLevel(sampler, uv, mip);
        }
    };

    struct Sampler {
        BindlessHandle handle;
        SamplerState load() {
            DescriptorHandle<SamplerState> sampler = DescriptorHandle<SamplerState>(uint2(handle.index, 0));
            return *nonuniform(sampler);
        }
    };
}
