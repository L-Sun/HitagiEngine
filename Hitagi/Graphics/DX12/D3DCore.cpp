#include "D3DCore.hpp"

namespace Hitagi::Graphics::DX12 {

Microsoft::WRL::ComPtr<ID3D12Device6> g_Device = nullptr;
CommandListManager                    g_CommandManager;

}  // namespace Hitagi::Graphics::DX12