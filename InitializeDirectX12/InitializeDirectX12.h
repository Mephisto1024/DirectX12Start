#pragma once

#define ThrowIfFailed(x)                                              \
{                                                                     \
    HRESULT hr__ = (x);												  \
    if(FAILED(hr__)) { throw DxException(hr__); }                     \
}

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

