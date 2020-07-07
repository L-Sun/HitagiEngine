

#include "DX12DriverAPI.hpp"

#ifdef _WIN32
#include <crtdbg.h>
#ifdef _DEBUG

#define new new (_NORMAL_BLOCK, __FILE__, __LINE__)
#endif
#endif

using namespace Hitagi::Graphics;

int main() {
    std::vector<int> a(1024, 100);
    {
        auto driver = std::make_unique<backend::DX12::DX12DriverAPI>();
        auto cb     = driver->CreateConstantBuffer(16, sizeof(size_t));
        auto vb     = driver->CreateVertexBuffer(a.size(), sizeof(int), reinterpret_cast<uint8_t*>(a.data()));
        for (size_t i = 0; i < 16; i++) {
            driver->UpdateConstantBuffer(cb, i, reinterpret_cast<const uint8_t*>(&i), sizeof(size_t));
        }
    }
    backend::DX12::DX12DriverAPI::ReportDebugLog();

#ifdef _WIN32
    _CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_LEAK_CHECK_DF);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);
#endif
    return 0;
}
