module viewer;

import std;
import asset;
import core;
import math;
import editor;
import render;

namespace hitagi::examples {

auto MakeForwardPass(bool textured) -> asset::MaterialPass {
    MaterialGraph graph;
    const auto    surface = graph.AddNode("surface", "surface_output",
                                          {{"base_color", ShaderValueType::Color},
                                           {"metallic", ShaderValueType::Float},
                                           {"roughness", ShaderValueType::Float},
                                           {"opacity", ShaderValueType::Float},
                                           {"alpha_cutout", ShaderValueType::UInt32},
                                           {"alpha_cutoff", ShaderValueType::Float},
                                           {"emissive", ShaderValueType::Float3}},
                                          {{"out", ShaderValueType::Surface}});

    if (textured) {
        const auto texture =
            graph.AddNode("base_color_texture", "parameter", {}, {{"texture", ShaderValueType::Texture2D}});
        const auto sample =
            graph.AddNode("sample_base_color", "texture_sample", {{"texture", ShaderValueType::Texture2D}},
                          {{"rgba", ShaderValueType::Color}});
        graph.AddEdge(texture, "texture", sample, "texture");
        graph.AddEdge(sample, "rgba", surface, "base_color");
    } else {
        const auto color = graph.AddNode("base_color", "parameter", {}, {{"value", ShaderValueType::Color}});
        graph.AddEdge(color, "value", surface, "base_color");
    }

    for (const auto name : {"metallic", "roughness", "opacity", "alpha_cutoff"}) {
        const auto parameter = graph.AddNode(name, "parameter", {}, {{"value", ShaderValueType::Float}});
        graph.AddEdge(parameter, "value", surface, name);
    }
    const auto alpha_cutout = graph.AddNode("alpha_cutout", "parameter", {}, {{"value", ShaderValueType::UInt32}});
    graph.AddEdge(alpha_cutout, "value", surface, "alpha_cutout");
    const auto emissive =
        graph.AddNode("emissive_color", "parameter", {}, {{"value", ShaderValueType::Color}});
    const auto emissive_rgb = graph.AddNode("emissive_rgb", "swizzle", {{"in", ShaderValueType::Color}},
                                            {{"rgb", ShaderValueType::Float3}});
    graph.AddEdge(emissive, "value", emissive_rgb, "in");
    graph.AddEdge(emissive_rgb, "rgb", surface, "emissive");
    graph.AddTerminal("surface", surface, "out", ShaderValueType::Surface);

    auto result = CompileMaterialGraph(graph, render::pbr::PbrForwardPassTemplate(),
                                       render::pbr::PbrMaterialDefaults(),
                                       {.name = textured ? "UsdViewerTextured" : "UsdViewerScalar"});
    if (!result) throw std::runtime_error("Cannot compile viewer material graph");
    return std::move(result.pass);
}

auto MakeGBufferPass() -> asset::MaterialPass {
    auto defaults = render::pbr::PbrMaterialDefaults();
    defaults.push_back({.name = "_padding0", .value = 0.0f});
    defaults.push_back({.name = "_padding1", .value = 0.0f});

    // Field order is the MaterialData ABI in deferred_gbuffer.slang.
    asset::MaterialPass pass{.pass_contract = "GBuffer"};
    for (const auto name :
         {"base_color", "metallic", "roughness", "occlusion", "_padding0", "emissive_color", "opacity",
          "alpha_cutout", "alpha_cutoff", "_padding1", "base_color_texture", "metallic_roughness_texture",
          "normal_texture", "occlusion_texture", "emissive_texture"}) {
        const auto parameter = std::ranges::find(defaults, name, &asset::MaterialParameter::name);
        pass.bindings.emplace_back(name, parameter->value);
    }
    return pass;
}

auto ConvertMaterial(const asset::Material& source, const std::array<asset::MaterialPass, 2>& forward,
                     const asset::MaterialPass&                   gbuffer,
                     const std::pmr::vector<asset::MaterialPass>& toon) -> std::shared_ptr<asset::Material> {
    render::pbr::PbrMaterialDesc desc;
    if (const auto color = source.GetParameter<math::vec3f>("diffuseColor"))
        desc.base_color = math::Color{*color, 1.0f};
    if (const auto color = source.GetParameter<math::vec3f>("emissiveColor"))
        desc.emissive_color = math::Color{*color, 1.0f};
    desc.metallic  = source.GetParameter<float>("metallic").value_or(desc.metallic);
    desc.roughness = source.GetParameter<float>("roughness").value_or(desc.roughness);
    desc.opacity   = source.GetParameter<float>("opacity").value_or(desc.opacity);
    if (const auto threshold = source.GetParameter<float>("opacityThreshold")) {
        desc.alpha_cutout = *threshold > 0.0f;
        desc.alpha_cutoff = *threshold;
    }
    desc.base_color_texture =
        source.GetParameter<std::shared_ptr<asset::Texture>>("diffuseColor").value_or(nullptr);

    auto parameters = render::pbr::CreatePbrMaterialParameters(desc);
    auto toon_desc = render::toon::ToonMaterialDesc{};
    // Optional authoring inputs on the USD surface; avoid guessing skin/eye
    // behavior from material names or applying a skin ramp to every surface.
    if (const auto color = source.GetParameter<math::vec3f>("hitagiToonShadowColor"))
        toon_desc.shadow_color = math::Color{*color, 1.0f};
    if (const auto color = source.GetParameter<math::vec3f>("hitagiToonOutlineColor"))
        toon_desc.outline_color = math::Color{*color, 1.0f};
    toon_desc.shadow_threshold = source.GetParameter<float>("hitagiToonShadowThreshold").value_or(toon_desc.shadow_threshold);
    toon_desc.light_threshold = source.GetParameter<float>("hitagiToonLightThreshold").value_or(toon_desc.light_threshold);
    toon_desc.band_softness = source.GetParameter<float>("hitagiToonBandSoftness").value_or(toon_desc.band_softness);
    toon_desc.specular_strength = source.GetParameter<float>("hitagiToonSpecularStrength").value_or(toon_desc.specular_strength);
    toon_desc.rim_strength = source.GetParameter<float>("hitagiToonRimStrength").value_or(toon_desc.rim_strength);
    toon_desc.outline_width = source.GetParameter<float>("hitagiToonOutlineWidth").value_or(toon_desc.outline_width);
    for (const auto& parameter : render::toon::CreateToonMaterialParameters(toon_desc)) {
        if (std::ranges::find(parameters, parameter.name, &asset::MaterialParameter::name) == parameters.end())
            parameters.emplace_back(parameter);
    }
    parameters.push_back({.name = "_padding0", .value = 0.0f});
    parameters.push_back({.name = "_padding1", .value = 0.0f});
    std::pmr::vector<asset::MaterialPass> passes{
        forward[desc.base_color_texture ? 1 : 0],
        gbuffer,
    };
    passes.insert(passes.end(), toon.begin(), toon.end());
    return std::make_shared<asset::Material>(std::move(parameters), std::move(passes), source.GetName());
}

}  // namespace hitagi::examples

namespace hitagi::examples {

auto MakeSceneImportPolicy() -> UsdImportPolicy {
    const std::array forward{MakeForwardPass(false), MakeForwardPass(true)};
    const auto       gbuffer  = MakeGBufferPass();
    const auto       toon     = render::toon::CreateToonMaterialPasses();
    const auto       fallback = ConvertMaterial(asset::Material({}, {}, "Default"), forward, gbuffer, toon);
    return {
        .material_getter    = [fallback](std::string_view) { return fallback; },
        .material_processor = [forward, gbuffer, toon](EditorMaterial& material) { material.material = ConvertMaterial(*material.material, forward, gbuffer, toon); },
    };
}

}  // namespace hitagi::examples
