#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

// STL
#include <iostream>
#include <cassert>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <cmath>
#include <functional>
#include <queue>
#include <mutex>
#include <array>
#include <memory_resource>
#include <bitset>

// DX12
#include "dx_helper.h"
#include "d3dx12.h"
#include <d3dcompiler.h>
#include <d3d12shader.h>
#include <dxcapi.h>
#include <dxgi1_6.h>

#undef near
#undef far

using namespace Microsoft::WRL;

#define D3D12_GPU_VIRTUAL_ADDRESS_NULL ((D3D12_GPU_VIRTUAL_ADDRESS)0)
#define D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN ((D3D12_GPU_VIRTUAL_ADDRESS)-1)