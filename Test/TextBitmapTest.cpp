#include <iostream>
#include <sstream>
#include <ft2build.h>
#include <iomanip>
#include FT_FREETYPE_H

#include "AssetLoader.hpp"
using namespace std;
using namespace Hitagi;

#define _CRTDBG_MAP_ALLOC
#ifdef _WIN32
#include <crtdbg.h>
#ifdef _DEBUG

#define new new (_NORMAL_BLOCK, __FILE__, __LINE__)
#endif
#endif

namespace Hitagi {
std::unique_ptr<MemoryManager> g_MemoryManager(new MemoryManager);
std::unique_ptr<AssetLoader>   g_AssetLoader(new AssetLoader);
}  // namespace Hitagi
void Init() {
    g_MemoryManager->Initialize();
    g_AssetLoader->Initialize();
}
void Finalize(string_view text) {
    cout << text << endl;
    g_AssetLoader->Finalize();
    g_MemoryManager->Finalize();
}

int main(int argc, char const* argv[]) {
    Init();
    {
        Buffer fontBuffer = g_AssetLoader->SyncOpenAndReadBinary("Asset/Fonts/Hasklig-Light.otf");

        FT_Library library;  // handle to library
        FT_Face    face;     // handle to face object

        FT_Error error;
        if (error = FT_Init_FreeType(&library); error != 0) {
            Finalize("Initialize FreeType failed!");
            return -1;
        }
        error = FT_New_Memory_Face(library, fontBuffer.GetData(), fontBuffer.GetDataSize(), 0, &face);
        if (error) {
            Finalize("Load font face failed!");
            return -1;
        }

        error = FT_Set_Char_Size(face, 0, 16 * 64, 300, 300);
        if (error) {
            Finalize("Set char size failed.");
            return -1;
        }
        auto glyph_index = FT_Get_Char_Index(face, 'a');
        error            = FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
        error            = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
        auto bitmap      = face->glyph->bitmap;

        for (size_t i = 0; i < bitmap.rows; i++) {
            for (size_t j = 0; j < bitmap.width; j++) {
                cout << setw(2)
                     << (static_cast<unsigned>(reinterpret_cast<uint8_t*>(bitmap.buffer)[i * bitmap.pitch + j]) > 0
                             ? 1
                             : 0);
            }
            cout << '\n';
        }
        FT_Done_Face(face);
        FT_Done_FreeType(library);
    }
    Finalize("");
#ifdef _WIN32
    _CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_LEAK_CHECK_DF);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);
#endif

    return 0;
}
