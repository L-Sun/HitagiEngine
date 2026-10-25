export module gfx:utilities;
import std;
import core;
import gfx.base;

export namespace hitagi::gfx {
auto create_device(Device::Type type, std::string_view name = "") -> std::unique_ptr<Device>;

auto readback_texture(
    Device&                 device,
    CommandQueues&          queues,
    BindlessUtils&          bindings,
    Texture&                texture,
    TextureSubresourceLayer layer = {}) -> core::Buffer;

}  // namespace hitagi::gfx
