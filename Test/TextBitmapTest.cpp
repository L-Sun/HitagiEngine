#include <iostream>
#include <sstream>
#include <ft2build.h>
#include <iomanip>
#include FT_FREETYPE_H

#include "MemoryManager.hpp"
#include "FileIOManager.hpp"
using namespace std;
using namespace Hitagi;

#define _CRTDBG_MAP_ALLOC
#ifdef _WIN32
#include <crtdbg.h>
#ifdef _DEBUG

#define new new (_NORMAL_BLOCK, __FILE__, __LINE__)
#endif
#endif

void init() {
    g_MemoryManager->Initialize();
    g_FileIoManager->Initialize();
}
void finalize(string_view text) {
    cout << text << endl;
    g_FileIoManager->Finalize();
    g_MemoryManager->Finalize();
}

int main(int argc, char const* argv[]) {
    init();
    {
        auto font_buffer = g_FileIoManager->SyncOpenAndReadBinary("Assets/Fonts/Hasklig-Light.otf");

        FT_Library library = nullptr;  // handle to library
        FT_Face    face    = nullptr;  // handle to face object

        FT_Error error = 0;
        if (error = FT_Init_FreeType(&library); error != 0) {
            finalize("Initialize FreeType failed!");
            return -1;
        }
        error = FT_New_Memory_Face(library, font_buffer.GetData(), font_buffer.GetDataSize(), 0, &face);
        if (error) {
            finalize("Load font face failed!");
            return -1;
        }

        error = FT_Set_Char_Size(face, 0, 16 * 64, 300, 300);
        if (error) {
            finalize("Set char size failed.");
            return -1;
        }
        auto glyph_index = FT_Get_Char_Index(face, 'a');
        if ((error = FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT)) != 0) return error;
        if ((error = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL)) != 0) return error;
        auto bitmap = face->glyph->bitmap;

        for (size_t i = 0; i < bitmap.rows; i++) {
            for (size_t j = 0; j < bitmap.width; j++) {
                cout << setw(2)
                     << (static_cast<size_t>(reinterpret_cast<uint8_t*>(bitmap.buffer)[i * bitmap.pitch + j]) > 0
                             ? 1
                             : 0);
            }
            cout << '\n';
        }
        FT_Done_Face(face);
        FT_Done_FreeType(library);
    }
    finalize("");
#ifdef _WIN32
    _CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_LEAK_CHECK_DF);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);
#endif

    return 0;
}
