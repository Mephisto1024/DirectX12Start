#pragma once

#include <d3d12.h>	/*包含函数：
D3D12CreateDevice()
D3D12GetDebugInterface()
类：
ID3D12Device4
*/
#include "d3dx12.h"/*包含 创建RT视图所需的句柄 CD3DX12_CPU_DESCRIPTOR_HANDLE */
#define ThrowIfFailed(x)                                              \
{                                                                     \
    HRESULT hr__ = (x);												  \
    if(FAILED(hr__)) { throw DxException(hr__); }                     \
}

// Set true to use 4X MSAA.   The default is false.
bool msaaState = false;    // 4X MSAA enabled
UINT msaaQuality = 0;      // quality level of 4X MSAA

int width = 800;
int height = 600;
const int SwapChainBufferCount = 2;
int currBackBuffer = 0;
UINT rtvDescriptorSize = 0;

D3D12_VIEWPORT ScreenViewport;
D3D12_RECT ScissorRect;

class DxException
{
public:
	DxException(HRESULT hr) :hrError(hr)
	{

	}
	HRESULT Error() const
	{
		return hrError;
	}
private:
	const HRESULT hrError;

};

