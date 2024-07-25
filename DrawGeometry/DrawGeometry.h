#pragma once

#include <d3d12.h>	/*����������
D3D12CreateDevice()
D3D12GetDebugInterface()
�ࣺ
ID3D12Device4
*/
#include "d3dx12.h"/*���� ����RT��ͼ����ľ�� CD3DX12_CPU_DESCRIPTOR_HANDLE */
#define WIN32_LEAN_AND_MEAN
#include<windows.h>
#include"DrawGeometry.h"
#include <wrl.h>	/*windows Runtime C++ Template Library������ʹ��ComPtr*/

#include<dxgi1_6.h>/*������
IDXGIFactory7

*/
#include<DirectXMath.h> /* ���� XMFLOAT3 */

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

    // ����Ĭ�ϻ�������Դ��Ĭ�϶�
    CD3DX12_HEAP_PROPERTIES defaultHeapProps(D3D12_HEAP_TYPE_DEFAULT);    //����Ҫ����ֵ
    ThrowIfFailed(device->CreateCommittedResource(
        &defaultHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(defaultBuffer.GetAddressOf())));

    // Ϊ�˽�CPU���ڴ��е����ݸ��Ƶ�Ĭ�ϻ����������ǻ���Ҫ����һ�������н�λ�õ��ϴ���
    CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
    ThrowIfFailed(device->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(uploadBuffer.GetAddressOf())));


    // �ṹ������������Ҫ���Ƶ�Ĭ�ϻ����������ݡ�
    D3D12_SUBRESOURCE_DATA subResourceData = {};
    subResourceData.pData = initData;        //��������
    subResourceData.RowPitch = byteSize;
    subResourceData.SlicePitch = subResourceData.RowPitch;

    /* Ҫ�뽫���ݸ��Ƶ�Ĭ�ϻ�������Դ
       UpdateSubresources�������Ƚ����ݴ�CPU�˵��ڴ��и��Ƶ��ϴ���
       �ٵ���CopyBufferRegion�������ϴ����ڵ����ݸ��Ƶ�Ĭ�϶�(defaultBuffer)��
    */
    //��ͨ��״̬תΪ��������Ŀ��
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
    /*ResourceBarrier �����Ĺ���ԭ��
    ������ResourceBarrier����ʱ��Ӧ�ó�����������б�ID3D12GraphicsCommandList�������һ����Դ�ϰ���
    ������GPU�ڼ��������������֮ǰ����Ҫȷ�����жԸ���Դ�ķ��ʲ����Ѿ���ɣ�������״̬ת����*/

    // Note: uploadBuffer has to be kept alive after the above function calls because
    // the command list has not been executed yet that performs the actual copy.
    // The caller can Release the uploadBuffer after it knows the copy has been executed.


    return defaultBuffer;
}

