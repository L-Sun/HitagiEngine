export module gfx;
export import gfx.base;
export import gfx.render_graph;

import std;

export namespace hitagi::gfx {
auto create_device(Device::Type type, std::string_view name = "") -> std::unique_ptr<Device>;
}  // namespace hitagi::gfx