#pragma once

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

