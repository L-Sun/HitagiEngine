#include "imfilebrowser.hpp"
#include <windows.h>

std::uint32_t ImGui::FileBrowser::GetDrivesBitMask() {
    DWORD    mask = GetLogicalDrives();
    uint32_t ret  = 0;
    for (int i = 0; i < 26; ++i) {
        if (!(mask & (1 << i))) {
            continue;
        }
        std::array rootName = {static_cast<char>('A' + i), ':', '\\', '\0'};
        UINT       type     = GetDriveTypeA(rootName.data());
        if (type == DRIVE_REMOVABLE || type == DRIVE_FIXED || type == DRIVE_REMOTE) {
            ret |= (1 << i);
        }
    }
    return ret;
}