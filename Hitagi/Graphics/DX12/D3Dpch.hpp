#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

// STL
#include <iostream>
#include <cassert>
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

// DX12
#include "DXHelper.hpp"
#include "d3dx12.h"
#include <dxgi1_6.h>

using namespace Microsoft::WRL;

#define D3D12_GPU_VIRTUAL_ADDRESS_NULL ((D3D12_GPU_VIRTUAL_ADDRESS)0)
#define D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN ((D3D12_GPU_VIRTUAL_ADDRESS)-1)