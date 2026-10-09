module;
#include "imfilebrowser.hpp"

export module interop.imfilebrowser;

export namespace ImGui {
using ::ImGui::FileBrowser;
}
export {
    using ::ImGuiFileBrowserFlags;
    using ::ImGuiFileBrowserFlags_;
    using enum ImGuiFileBrowserFlags_;
}
