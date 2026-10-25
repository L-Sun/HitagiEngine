export module gfx.metal:device;
import interop.metal;
import interop.spdlog;

import std;
import gfx.base;
import :types;
import :utils;

export namespace hitagi::gfx {

class MetalDevice final : public Device {
public:
    explicit MetalDevice(std::string_view name);
    void BeginFrame() final;
    void EndFrame() noexcept final;

    // MTLDevice is shared by the OS. The address of this owning RAII handle also
    // identifies the engine device for foreign-resource/queue validation.
    inline auto& GetDevice() const noexcept { return m_Device; }
    inline auto& GetCompiler() const noexcept { return *m_Compiler.get(); }

private:
    // Fallback for initialization and standalone device users. Never drain this
    // from BeginFrame: a native event callback may have pushed a younger Cocoa pool.
    ns::SharedPtr<ns::AutoreleasePool> m_AutoreleasePool;
    ns::SharedPtr<mtl::Device>         m_Device;
    ns::SharedPtr<mtl4::Compiler>      m_Compiler;
    // Younger than the native pool surrounding a live-resize callback. Only
    // EndFrame drains it; the device-lifetime fallback above stays untouched.
    ns::SharedPtr<ns::AutoreleasePool> m_FramePool;
};

}  // namespace hitagi::gfx

namespace hitagi::gfx {

MetalDevice::MetalDevice(std::string_view name)
    : Device(Type::Metal, name),
      m_AutoreleasePool(ns::TransferPtr(ns::AutoreleasePool::alloc()->init())),
      m_Device(ns::TransferPtr(mtl::CreateSystemDefaultDevice())) {
    check_metal(m_Logger, m_Device.get() != nullptr, "No Metal device available");
    m_Logger->trace("Create Metal 4 compiler...");
    ns::Error* error      = nullptr;
    auto       descriptor = ns::TransferPtr(mtl4::CompilerDescriptor::alloc()->init());
    m_Compiler            = ns::TransferPtr(m_Device->newCompiler(descriptor.get(), &error));
    check_metal(m_Logger, m_Compiler.get() != nullptr, "Create Metal 4 compiler failed", error);
    m_Logger->trace("Pick Metal device: {}", m_Device->name()->utf8String());
}

void MetalDevice::BeginFrame() {
    if (m_FramePool) throw std::logic_error("Metal device frame is already active");
    m_FramePool = ns::TransferPtr(ns::AutoreleasePool::alloc()->init());
    Device::BeginFrame();
}

void MetalDevice::EndFrame() noexcept {
    m_FramePool.reset();
}

}  // namespace hitagi::gfx
