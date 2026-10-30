module;
#include "interop/tracy_macros.hpp"

module viewer;
import interop.tracy;

import std;
import utils;
import asset;
import math;
import gfx;
import render;

namespace hitagi::examples {

void USDViewer::RenderFrame() {
    if (m_App.WindowsMinimized() || !m_App.GetWindowWidth() || !m_App.GetWindowHeight()) {
        m_Gui.Compose();
    }
    // RenderApp runs after the main World on this thread; viewer state stays alive.
    m_Rendering.SetFrame([this](rg::RenderGraph& graph, rg::TextureHandle output) {
        return BuildFrame(graph, output);
    });
}

auto USDViewer::BuildFrame(rg::RenderGraph& graph, rg::TextureHandle output) -> rg::TextureHandle {
    if (m_Scene && m_Viewport.z > 0 && m_Viewport.w > 0) {
        std::vector<render::pbr::RenderDrawItem>      pbr_items;
        std::vector<render::deferred::RenderDrawItem> deferred_items;
        {
        ZoneScopedNS("Viewer.CollectDrawItems", 0);
        for (auto entity : m_Scene->GetWorld().GetEntityManager().GetEntities<asset::MeshComponent, asset::Transform>()) {
            if (!Visible(entity)) continue;
            const auto& mesh = entity.Get<asset::MeshComponent>().mesh;
            if (!mesh || mesh->Empty()) continue;
            pbr_items.emplace_back(render::pbr::RenderDrawItem::FromEntity(entity));
            deferred_items.emplace_back(render::deferred::RenderDrawItem::FromEntity(
                entity, static_cast<std::uint32_t>(deferred_items.size() + 1)));
        }
        }
        const auto  rect   = m_Gui.DisplayRect(m_Viewport);
        const auto  target = graph.Create(gfx::TextureDesc{
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
        if (m_Renderer == RendererType::Pbr) {
            const std::array                 lights{render::pbr::RenderLight{
                .position = light_position, .color = math::Color::White(), .intensity = intensity}};
            const render::pbr::RenderRequest request{
                .frame  = {.view = {.camera_position = m_Orbit.Eye(), .view = view, .projection = projection}, .draw_items = pbr_items, .lights = lights},
                .target = target,
            };
            color = m_Pbr.Build(graph, request).color;
        } else if (m_Renderer == RendererType::Toon) {
            // A lower frontal studio key keeps facial planes readable. Material
            // ramps provide the fill instead of a dark blue band across the chin.
            const auto toon_light = math::normalize(-m_Orbit.LookDirection() +
                (math::vec3f(0, 1, 0) * 0.3f - right * 0.25f) * distance);
            const render::toon::RenderRequest request{
                .frame  = {.view = {.camera_position = m_Orbit.Eye(), .view = view, .projection = projection}, .draw_items = pbr_items, .light = {.direction_to_light = toon_light, .intensity = m_Light}},
                .target = target,
            };
            color = m_Toon.Build(graph, request).color;
        } else {
            const std::array                      lights{render::deferred::RenderLight{
                .position = light_position, .color = math::Color::White(), .intensity = intensity}};
            const render::deferred::RenderRequest request{
                .frame      = {.view = {.camera_position = m_Orbit.Eye(), .view = view, .projection = projection}, .draw_items = deferred_items, .lights = lights},
                .target     = target,
                .debug_view = m_Debug,
            };
            color = m_Deferred.Build(graph, request).color;
        }
        m_Gui.DrawTexture(color, m_Viewport);
    }
    m_Gui.Compose();
    if (m_OverlayLines.empty()) {
        m_GuiDraw.GuiPass(graph, output, m_Gui.GetDrawData(), true);
        return graph.MoveFrom(output);
    }
    // Diagnostic lines are projected with the same camera as the scene. They
    // deliberately remain visible through meshes and are clipped to the viewport.
    // GuiPass retains the draw-data address until graph execution.
    auto& draw_data = m_OverlayDrawData;
    draw_data = m_Gui.GetDrawData();
    if (!m_OverlayLines.empty() && m_Viewport.z > 0 && m_Viewport.w > 0) {
        const auto rect = m_Gui.DisplayRect(m_Viewport);
        const auto view = math::look_at(m_Orbit.Eye(), m_Orbit.LookDirection(), math::vec3f(0, 1, 0));
        const auto projection = math::perspective(2.0f * std::atan(std::tan(m_Camera.horizontal_fov * .5f) / m_Camera.aspect), m_Camera.aspect, m_Camera.near_clip, m_Camera.far_clip);
        gui::GuiDrawList lines;
        for (const auto& line : m_OverlayLines) {
            const auto a = projection * view * math::vec4f(line.from, 1);
            const auto b = projection * view * math::vec4f(line.to, 1);
            if (a.w <= 0 || b.w <= 0 || a.z < 0 || b.z < 0) continue;
            const math::vec2f p{rect.x + (a.x / a.w + 1) * rect.z * .5f, rect.y + (1 - a.y / a.w) * rect.w * .5f};
            const math::vec2f q{rect.x + (b.x / b.w + 1) * rect.z * .5f, rect.y + (1 - b.y / b.w) * rect.w * .5f};
            const auto d = q - p;
            if (d.norm() < .01f) continue;
            const math::vec2f normal{-d.y / d.norm(), d.x / d.norm()};
            const auto base = static_cast<std::uint32_t>(lines.vertices.size());
            for (const auto& position : {p + normal, p - normal, q - normal, q + normal})
                lines.vertices.push_back({.position = position, .color = line.color});
            for (auto index : {0u, 1u, 2u, 0u, 2u, 3u}) lines.indices.push_back(base + index);
        }
        lines.commands.push_back({.element_count = static_cast<std::uint32_t>(lines.indices.size()), .clip_rect = {rect.x, rect.y, rect.x + rect.z, rect.y + rect.w}});
        draw_data.draw_lists.push_back(std::move(lines));
    }
    m_GuiDraw.GuiPass(graph, output, draw_data, true);
    return graph.MoveFrom(output);
}

auto USDViewer::Capture() -> asset::ImageData {
    const auto& output = m_Rendering.GetWindowTarget();
    if (!output) throw std::logic_error("Render a frame before capture");
    const auto& desc = output->GetDesc();
    auto        pixels =
        gfx::readback_texture(m_Resources.device, m_Resources.queues, m_Resources.bindings, *output);
    auto format = desc.format;
    if (format == gfx::Format::B8G8R8A8_UNORM || format == gfx::Format::B8G8R8A8_UNORM_SRGB) {
        auto bytes = pixels.Span<std::byte>();
        for (std::size_t i = 0; i < bytes.size(); i += 4) std::swap(bytes[i], bytes[i + 2]);
        format = gfx::Format::R8G8B8A8_UNORM;
    }
    return {.width = desc.width, .height = desc.height, .format = format, .data = std::move(pixels)};
}

}  // namespace hitagi::examples
