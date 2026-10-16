module;

#include "interop/tracy_macros.hpp"

module editor;
import interop.tracy;

using namespace hitagi::math;
using namespace hitagi::asset;
using namespace std::literals;

namespace hitagi {

auto BuildEditorRenderGraphDebugSnapshot(std::string_view dot) -> EditorRenderGraphDebugSnapshot {
    EditorRenderGraphDebugSnapshot snapshot;

    std::size_t cursor = 0;
    while (cursor < dot.size()) {
        const auto line_end = dot.find('\n', cursor);
        const auto line     = dot.substr(cursor, line_end == std::string_view::npos ? dot.size() - cursor : line_end - cursor);
        cursor              = line_end == std::string_view::npos ? dot.size() : line_end + 1;

        const auto bracket = line.find('[');
        if (bracket == std::string_view::npos || line.find("->") != std::string_view::npos) continue;

        const auto id_text = line.substr(0, bracket);
        std::uint64_t handle = 0;
        auto first_digit = id_text.find_first_of("0123456789");
        if (first_digit == std::string_view::npos) continue;
        for (auto ch : id_text.substr(first_digit)) {
            if (ch < '0' || ch > '9') break;
            handle = handle * 10 + static_cast<std::uint64_t>(ch - '0');
        }

        const auto label = line.find("label=\"", bracket);
        if (label == std::string_view::npos) continue;
        const auto name_begin = label + std::string_view("label=\"").size();
        const auto name_end   = line.find("\\nhandle:", name_begin);
        if (name_end == std::string_view::npos) continue;

        EditorRenderGraphDebugNode node{
            .handle   = handle,
            .name     = std::pmr::string(line.substr(name_begin, name_end - name_begin)),
            .resource = line.find("shape=box", bracket) != std::string_view::npos,
        };
        if (node.resource) {
            snapshot.resources.emplace_back(std::move(node));
        } else {
            snapshot.passes.emplace_back(std::move(node));
        }
    }

    return snapshot;
}

}  // namespace hitagi
