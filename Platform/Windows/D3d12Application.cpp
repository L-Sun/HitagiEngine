#include <tchar.h>
#include "D3D12Application.hpp"
#include "D3D/D3D12GraphicsManager.hpp"

using namespace My;

void D3D12Application::Tick() {
    WindowsApplication::Tick();

    // Present the back buffer to the screen since rendering is complete.
    // SwapBuffers(m_hDc);
}
