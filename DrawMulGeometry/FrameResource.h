#pragma once
#include"MfstUtil.h"
#include<DirectXMath.h>
// Lightweight structure stores parameters to draw a shape.
struct ObjectConstants
{
    DirectX::XMFLOAT4X4 World = Identity4x4();
};

struct PassConstants
{
    DirectX::XMFLOAT4X4 View = Identity4x4();
    DirectX::XMFLOAT4X4 InvView = Identity4x4();
    DirectX::XMFLOAT4X4 Proj = Identity4x4();
    DirectX::XMFLOAT4X4 InvProj = Identity4x4();
    DirectX::XMFLOAT4X4 ViewProj = Identity4x4();
    DirectX::XMFLOAT4X4 InvViewProj = Identity4x4();
    DirectX::XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
    float cbPerObjectPad1 = 0.0f;
    DirectX::XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };
    DirectX::XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f };
    float NearZ = 0.0f;
    float FarZ = 0.0f;
    float TotalTime = 0.0f;
    float DeltaTime = 0.0f;
};

struct Vertex
{
    DirectX::XMFLOAT3 Pos;
    DirectX::XMFLOAT4 Color;
};

// 存储CPU构建每帧的命令列表所需的资源
struct FrameResource
{
public:

    FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount);
    FrameResource(const FrameResource& rhs) = delete;
    FrameResource& operator=(const FrameResource& rhs) = delete;
    ~FrameResource();

    // 在GPU完成命令处理之前，我们无法重置分配器。所以每个帧都需要自己的分配器。
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;

    // 在GPU完成对引用它的命令的处理之前，我们不能更新cbuffer。所以每一帧都需要自己的cbuffer。
    std::unique_ptr<UploadBuffer<PassConstants>> PassCB = nullptr;
    std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB = nullptr;

    // 通过栅栏值，将命令标记到此栅栏点。这让我们可以检查这些帧资源是否仍在被GPU使用。
    UINT64 Fence = 0;
};
FrameResource::FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount)
{
    ThrowIfFailed(device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(CmdListAlloc.GetAddressOf())));

    PassCB = std::make_unique<UploadBuffer<PassConstants>>(device, passCount, true);
    ObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(device, objectCount, true);
}

FrameResource::~FrameResource()
{

}