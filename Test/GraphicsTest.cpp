#include "PipelineState.hpp"
#include <fmt/format.h>
#include <iostream>

using namespace Hitagi::Graphics;

int main(int argc, char const* argv[]) {
    RootSignature sig("root signature test");
    sig.Add("", ShaderVariableType::CBV, 0, 0)
        .Add("", ShaderVariableType::SRV, 0, 1)
        .Add("", ShaderVariableType::CBV, 1, 0)
        .Add("", ShaderVariableType::UAV, 2, 0)
        .Add("", ShaderVariableType::Sampler, 1, 0)
        .Add("", ShaderVariableType::CBV, 2, 0)
        .Add("", ShaderVariableType::Sampler, 0, 0);

    std::unordered_map<ShaderVariableType, std::string> typeInfo = {
        {ShaderVariableType::CBV, "c"},
        {ShaderVariableType::SRV, "t"},
        {ShaderVariableType::UAV, "u"},
        {ShaderVariableType::Sampler, "s"},
    };
    std::unordered_map<ShaderVisibility, std::string> visibilityInfo = {
        {ShaderVisibility::All, "All"},
        {ShaderVisibility::Vertex, "Vertex"},
        {ShaderVisibility::Hull, "Hull"},
        {ShaderVisibility::Domain, "Domain"},
        {ShaderVisibility::Geometry, "Geometry"},
        {ShaderVisibility::Pixel, "Pixel"},
    };

    auto& parameterTable = sig.GetParametes();

    for (auto&& parameter : parameterTable) {
        std::cout << fmt::format(
                         "{} : ({}{},{})",
                         visibilityInfo[parameter.visibility],
                         typeInfo[parameter.type],
                         parameter.registerIndex,
                         parameter.space)
                  << std::endl;
    }
    using Iter = std::decay_t<decltype(parameterTable.begin())>;
    struct Range {
        Iter   iter;
        size_t count;
    };
    std::vector<Range> ranges;
    for (auto iter = parameterTable.begin(); iter != parameterTable.end(); iter++) {
        if (ranges.empty()) {
            ranges.emplace_back(Range{iter, 1});
            continue;
        }
        if (auto& p = *(ranges.back().iter);
            p.visibility == iter->visibility && p.type == iter->type && p.space == iter->space) {
            // new range
            if (p.registerIndex + ranges.back().count < iter->registerIndex)
                ranges.emplace_back(Range{iter, 1});
            // fllowing the range
            else if (p.registerIndex + ranges.back().count == iter->registerIndex)
                ranges.back().count++;
            // [Error] in the range
            else
                throw std::logic_error("Has a same paramter");
        } else {
            ranges.emplace_back(Range{iter, 1});
        }
    }

    struct Table {
        ShaderVisibility    visibility;
        std::vector<size_t> ranges;
    };
    std::vector<Table> tables;
    for (size_t i = 0; i < ranges.size(); i++) {
        if (tables.empty()) {
            tables.push_back(Table{ranges[i].iter->visibility, {i}});
            continue;
        }
        if (tables.back().visibility == ranges[i].iter->visibility) {
            if (ranges[i].iter->type == ShaderVariableType::Sampler &&
                ranges[i].iter->type != ranges[tables.back().ranges.back()].iter->type) {
                // Sampler table
                tables.push_back(Table{ranges[i].iter->visibility, {i}});
            } else {
                tables.back().ranges.push_back(i);
            }
        } else {
            tables.push_back(Table{ranges[i].iter->visibility, {i}});
        }
    }

    return 0;
}
