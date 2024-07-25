#pragma once

#include <d3d12.h>	/*包含函数：
D3D12CreateDevice()
D3D12GetDebugInterface()
类：
ID3D12Device4
*/
#include "d3dx12.h"/*包含 创建RT视图所需的句柄 CD3DX12_CPU_DESCRIPTOR_HANDLE */
#define WIN32_LEAN_AND_MEAN
#include<windows.h>
#include"DrawGeometry.h"
#include <wrl.h>	/*windows Runtime C++ Template Library，方便使用ComPtr*/

#include<dxgi1_6.h>/*包含：
IDXGIFactory7

*/
#include<DirectXMath.h> /* 包含 XMFLOAT3 */

#include<unordered_map>
using namespace Microsoft::WRL;
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
struct Mesh
{
    Microsoft::WRL::ComPtr<ID3DBlob> VertexBufferCPU = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> IndexBufferCPU = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferGPU = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferGPU = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferUploader = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferUploader = nullptr;

    // Data about the buffers.
    UINT VertexByteStride = 0;
    UINT VertexBufferByteSize = 0;
    DXGI_FORMAT IndexFormat = DXGI_FORMAT_R16_UINT;
    UINT IndexBufferByteSize = 0;

    // A MeshGeometry may store multiple geometries in one vertex/index buffer.
    // Use this container to define the Submesh geometries so we can draw
    // the Submeshes individually.
    //std::unordered_map<std::string, SubmeshGeometry> DrawArgs;

    D3D12_VERTEX_BUFFER_VIEW VertexBufferView()const
    {
        D3D12_VERTEX_BUFFER_VIEW vbv;
        vbv.BufferLocation = VertexBufferGPU->GetGPUVirtualAddress();
        vbv.StrideInBytes = VertexByteStride;
        vbv.SizeInBytes = VertexBufferByteSize;

        return vbv;
    }

    D3D12_INDEX_BUFFER_VIEW IndexBufferView()const
    {
        D3D12_INDEX_BUFFER_VIEW ibv;
        ibv.BufferLocation = IndexBufferGPU->GetGPUVirtualAddress();
        ibv.Format = IndexFormat;
        ibv.SizeInBytes = IndexBufferByteSize;

        return ibv;
    }
};
Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefaultBuffer(
    ID3D12Device* device,
    ID3D12GraphicsCommandList* cmdList,
    const void* initData,
    UINT64 byteSize,
    Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer)
{
    ComPtr<ID3D12Resource> defaultBuffer;
    auto desc = CD3DX12_RESOURCE_DESC::Buffer(byteSize);

    // 创建默认缓冲区资源，默认堆
    CD3DX12_HEAP_PROPERTIES defaultHeapProps(D3D12_HEAP_TYPE_DEFAULT);    //报错，要求左值
    ThrowIfFailed(device->CreateCommittedResource(
        &defaultHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(defaultBuffer.GetAddressOf())));

    // 为了将CPU端内存中的数据复制到默认缓冲区，我们还需要创建一个处于中介位置的上传堆
    CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
    ThrowIfFailed(device->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(uploadBuffer.GetAddressOf())));


    // 结构体描述我们想要复制到默认缓冲区的数据。
    D3D12_SUBRESOURCE_DATA subResourceData = {};
    subResourceData.pData = initData;        //顶点数据
    subResourceData.RowPitch = byteSize;
    subResourceData.SlicePitch = subResourceData.RowPitch;

    /* 要想将数据复制到默认缓冲区资源
       UpdateSubresources函数会先将数据从CPU端的内存中复制到上传堆
       再调用CopyBufferRegion函数把上传堆内的数据复制到默认堆(defaultBuffer)中
    */
    //从通用状态转为用作复制目标
    auto resourceBarrier1 = CD3DX12_RESOURCE_BARRIER::Transition(
        defaultBuffer.Get(),
        D3D12_RESOURCE_STATE_COMMON, 
        D3D12_RESOURCE_STATE_COPY_DEST);
    cmdList->ResourceBarrier(1, &resourceBarrier1);

    UpdateSubresources<1>(cmdList, defaultBuffer.Get(), uploadBuffer.Get(), 0, 0, 1, &subResourceData);

    auto resourceBarrier2 = CD3DX12_RESOURCE_BARRIER::Transition(
        defaultBuffer.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_GENERIC_READ);
    cmdList->ResourceBarrier(1, &resourceBarrier2);
    /*ResourceBarrier 函数的工作原理
    当调用ResourceBarrier函数时，应用程序会向命令列表（ID3D12GraphicsCommandList）中添加一个资源障碍。
    这会告诉GPU在继续处理后续命令之前，需要确保所有对该资源的访问操作已经完成，并进行状态转换。*/

    // Note: uploadBuffer has to be kept alive after the above function calls because
    // the command list has not been executed yet that performs the actual copy.
    // The caller can Release the uploadBuffer after it knows the copy has been executed.


    return defaultBuffer;
}

