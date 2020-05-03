#pragma once
#include "CommandListManager.hpp"

namespace Hitagi::Graphics::DX12 {

extern Microsoft::WRL::ComPtr<ID3D12Device6> g_Device;
extern CommandListManager                    g_CommandManager;

}  // namespace Hitagi::Graphics::DX12