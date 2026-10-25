module viewer;

import std;
import utils;
import asset;
import math;
import gfx;
import render;

namespace hitagi::examples {

void USDViewer::RenderFrame() {
    const auto width  = m_App.GetWindowWidth();
    const auto height = m_App.GetWindowHeight();
    if (m_App.WindowSizeChanged() || (!m_App.WindowsMinimized() && (m_SwapChain->GetWidth() != width ||
                                                                    m_SwapChain->GetHeight() != height)))
        m_SwapChain->Resize();
    if (m_App.WindowsMinimized() || !width || !height) {
        m_Gui.Compose();
        return;
    }
    if (!m_Output || m_Output->GetDesc().width != width || m_Output->GetDesc().height != height) {
        m_Output = gfx::Texture::Create(
            m_Resources.device, m_Resources.queues, m_Resources.bindings,
            {
                .name        = "ViewerOutput",
                .width       = width,
                .height      = height,
                .format      = m_SwapChain->GetFormat(),
                .clear_value = math::Color(0.067f, 0.082f, 0.11f, 1.0f),
                .usages      = gfx::TextureUsageFlags::RenderTarget | gfx::TextureUsageFlags::CopySrc,
            });
    }
    if (m_Scene && m_Viewport.z > 0 && m_Viewport.w > 0) {
        std::vector<render::pbr::RenderDrawItem>      pbr_items;
        std::vector<render::deferred::RenderDrawItem> deferred_items;
        for (const auto& node : m_Nodes) {
            if (!Visible(node) || !node.entity.Has<asset::MeshComponent>()) continue;
            const auto& mesh = node.entity.Get<asset::MeshComponent>().mesh;
            if (!mesh || mesh->Empty()) continue;
            pbr_items.emplace_back(render::pbr::RenderDrawItem::FromEntity(node.entity));
            deferred_items.emplace_back(render::deferred::RenderDrawItem::FromEntity(
                node.entity, static_cast<std::uint32_t>(deferred_items.size() + 1)));
        }
        const auto  rect   = m_Gui.DisplayRect(m_Viewport);
        const auto  target = m_Graph.Create(gfx::TextureDesc{
            .name        = "SceneViewport",
            .width       = std::max(1u, static_cast<unsigned>(rect.z)),
            .height      = std::max(1u, static_cast<unsigned>(rect.w)),
            .format      = gfx::Format::R8G8B8A8_UNORM,
            .clear_value = math::Color(0.055f, 0.065f, 0.085f, 1.0f),
            .usages      = gfx::TextureUsageFlags::RenderTarget | gfx::TextureUsageFlags::SRV,
        });
        const auto& camera = m_Camera;
        const auto  view   = math::look_at(m_Orbit.Eye(), m_Orbit.LookDirection(), math::vec3f(0, 1, 0));
        const auto  projection =
            math::perspective(2.0f * std::atan(std::tan(camera.horizontal_fov * 0.5f) / camera.aspect),
                              camera.aspect, camera.near_clip, camera.far_clip);
        const float       distance       = m_Orbit.LookDirection().norm();
        const auto        right          = math::normalize(math::cross(m_Orbit.LookDirection(), math::vec3f(0, 1, 0)));
        const auto        light_position = m_Orbit.Eye() + (math::vec3f(0, 1, 0) * 0.7f - right * 0.45f) * distance;
        const float       intensity      = distance * distance * 4.0f * m_Light;
        rg::TextureHandle color;
        if (m_Pbr) {
            const std::array lights{render::pbr::RenderLight{
                .position = light_position, .color = math::Color::White(), .intensity = intensity}};
            const render::pbr::RenderRequest request{
                .frame  = {.view = {m_Orbit.Eye(), view, projection}, .draw_items = pbr_items, .lights = lights},
                .target = target,
            };
            color = m_Pbr->Build(m_Graph, request).color;
        } else if (m_Toon) {
            const render::toon::RenderRequest request{
                .frame  = {.view = {m_Orbit.Eye(), view, projection}, .draw_items = pbr_items, .light = {.direction_to_light = math::normalize(light_position - (m_Orbit.Eye() + m_Orbit.LookDirection())), .intensity = m_Light}},
                .target = target,
            };
            color = m_Toon->Build(m_Graph, request).color;
        } else {
            const std::array lights{render::deferred::RenderLight{
                .position = light_position, .color = math::Color::White(), .intensity = intensity}};
            const render::deferred::RenderRequest request{
                .frame      = {.view = {m_Orbit.Eye(), view, projection}, .draw_items = deferred_items, .lights = lights},
                .target     = target,
                .debug_view = m_Debug,
            };
            color = m_Deferred->Build(m_Graph, request).color;
        }
        m_Gui.DrawTexture(color, m_Viewport);
    }
    m_Gui.Compose();
    const auto output = m_Graph.Import(m_Output);
    m_GuiDraw.GuiPass(m_Graph, output, m_Gui.GetDrawData(), true);
    rg::PresentPassBuilder present(m_Graph);
    present.From(m_Graph.MoveFrom(output));
    present.SetSwapChain(m_SwapChain);
    present.Finish();
    if (!m_Graph.Compile()) throw std::runtime_error("Viewer graph compilation failed");
    m_Graph.Execute();
    m_Graph.ClearImportedResources();
    m_SwapChain->Present();
}

auto USDViewer::Capture() -> asset::ImageData {
    if (!m_Output) throw std::logic_error("Render a frame before capture");
    const auto& desc = m_Output->GetDesc();
    auto        pixels =
        gfx::readback_texture(m_Resources.device, m_Resources.queues, m_Resources.bindings, *m_Output);
    auto format = desc.format;
    if (format == gfx::Format::B8G8R8A8_UNORM || format == gfx::Format::B8G8R8A8_UNORM_SRGB) {
        auto bytes = pixels.Span<std::byte>();
        for (std::size_t i = 0; i < bytes.size(); i += 4) std::swap(bytes[i], bytes[i + 2]);
        format = gfx::Format::R8G8B8A8_UNORM;
    }
    return {.width = desc.width, .height = desc.height, .format = format, .data = std::move(pixels)};
}

}  // namespace hitagi::examples
