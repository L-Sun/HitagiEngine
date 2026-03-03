export module gfx;
export import gfx.base;
export import gfx.render_graph;

import std;
import core;

export namespace hitagi::gfx {
auto create_device(Device::Type type, std::string_view name = "") -> std::unique_ptr<Device>;

auto readback_texture(
    Device&                 device,
    Texture&                texture,
    TextureSubresourceLayer layer = {}
) -> core::Buffer;

}  // namespace hitagi::gfx