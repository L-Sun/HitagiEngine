export module gfx.metal:configs;
import std;

export namespace hitagi::gfx {

// Shared by CPU argument-buffer storage and SPIRV-Cross MSL binding remapping.
// The Metal 4 baseline is M4 (Apple9) or newer; its per-stage sampler limit is 500 K.
constexpr std::size_t   metal_resource_capacity            = 4096;
constexpr std::size_t   metal_sampler_capacity             = 500000;
constexpr std::uint32_t metal_buffer_binding               = 0;
constexpr std::uint32_t metal_texture1d_binding            = metal_resource_capacity;
constexpr std::uint32_t metal_texture_binding              = metal_resource_capacity * 2;
constexpr std::uint32_t metal_uint_texture_binding         = metal_resource_capacity * 3;
constexpr std::uint32_t metal_sint_texture_binding         = metal_resource_capacity * 4;
constexpr std::uint32_t metal_storage_texture1d_binding    = metal_resource_capacity * 5;
constexpr std::uint32_t metal_storage_texture_binding      = metal_resource_capacity * 6;
constexpr std::uint32_t metal_storage_uint_texture_binding = metal_resource_capacity * 7;
constexpr std::uint32_t metal_storage_sint_texture_binding = metal_resource_capacity * 8;
constexpr std::uint32_t metal_sampler_binding              = metal_resource_capacity * 9;
constexpr std::uint32_t metal_argument_buffer_slot         = 0;
constexpr std::uint32_t metal_bindless_meta_slot           = 1;
constexpr std::uint32_t metal_vertex_buffer_slot           = 2;
constexpr std::uint32_t metal_max_buffer_bindings          = 31;

}  // namespace hitagi::gfx
