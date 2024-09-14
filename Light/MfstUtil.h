#pragma once
#include<DirectXMath.h> /* °üº¬ XMFLOAT3 */
#include"MathHelper.h"
namespace Mfst
{
	struct ObjectConstants
	{
		DirectX::XMFLOAT4X4 WorldViewProj = MathHelper::Identity4x4();
	};

	static UINT CalcConstantBufferByteSize(UINT byteSize)
	{
		return (byteSize + 255) & ~255;
	}
}
