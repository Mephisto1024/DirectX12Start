#pragma once

#include "resource.h"
/* C++ Std */
#include<string>
#include<unordered_map>
/* Win32 */
#include <wrl.h>	/*windows Runtime C++ Template Library������ʹ��ComPtr*/
#include<windows.h>
#include<comdef.h>
#define WIN32_LEAN_AND_MEAN
/* DX12 */
#include <d3d12.h>	/*����������D3D12CreateDevice()*/
#include "d3dx12.h"/*���� ����RT��ͼ����ľ�� CD3DX12_CPU_DESCRIPTOR_HANDLE */
#include<dxgi1_6.h>/*������IDXGIFactory7*/
#include<DirectXMath.h> /* ���� XMFLOAT3 */
#include"MathHelper.h"
#include <d3dcompiler.h>
/* �Լ�д�� */

/* ���ӿ� */
#pragma comment(lib, "d3d12.lib")    //D3D12GetDebugInterface()
#pragma comment(lib, "dxgi.lib")    //CreateDXGIFactory()
#pragma comment(lib, "d3dcompiler.lib")

using namespace Microsoft::WRL;

#ifndef ThrowIfFailed
#define ThrowIfFailed(x)                                              \
{                                                                     \
    HRESULT hr__ = (x);                                               \
    std::wstring wfn = AnsiToWString(__FILE__);                       \
    if(FAILED(hr__)) { throw DxException(hr__, L#x, wfn, __LINE__); } \
}
#endif

bool InitDirect3D();
bool Initialize(HINSTANCE instanceHandle, int show);
int Run();
void Update();
void Draw();
void OnResize();
void CreateCommandObjects();
void CreateSwapChain();
void CreateDescriptorHeaps();
void FlushCommandQueue();

void BuildConstantBuffers();
void BuildRootSignature();
void BuildShadersAndInputLayout();
void BuildGeometry();
void BuildPSO();

void PopulateCommandList();

const int SwapChainBufferCount = 2;

D3D_FEATURE_LEVEL FeatureLevel = D3D_FEATURE_LEVEL_12_2;
ComPtr<IDXGIFactory4> DxgiFactory;
ComPtr<ID3D12Device6> Device;    //�Կ�
ComPtr<ID3D12Fence1> Fence;
ComPtr<ID3D12CommandAllocator> CommandAllocator;
ComPtr<ID3D12CommandQueue> CommandQueue;
ComPtr<ID3D12GraphicsCommandList> CommandList;
ComPtr<IDXGISwapChain1> SwapChain;
ComPtr<ID3D12DescriptorHeap> RtvHeap;
ComPtr<ID3D12DescriptorHeap> DsvHeap;
ComPtr<ID3D12DescriptorHeap> CbvHeap;
ComPtr<ID3D12Resource> SwapChainBuffer[SwapChainBufferCount];
ComPtr<ID3D12Resource> DepthStencilBuffer;
ComPtr<ID3D12RootSignature> RootSignature = nullptr;
ComPtr<ID3DBlob> VSByteCode = nullptr;
ComPtr<ID3DBlob> PSByteCode = nullptr;
ComPtr<ID3D12PipelineState> PSO = nullptr;

// Set true to use 4X MSAA.   The default is false.
bool msaaState = false;    // 4X MSAA enabled
UINT msaaQuality = 0;      // quality level of 4X MSAA

int width = 1280;
int height = 720;

int currBackBuffer = 0;
UINT rtvDescriptorSize = 0;
UINT CbvSrvUavDescriptorSize = 0;
UINT64 CurrentFence = 0;
DXGI_FORMAT BackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
DXGI_FORMAT DepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

D3D12_VIEWPORT ScreenViewport;
D3D12_RECT ScissorRect;
/* Win32 */
bool Paused = false;
bool Minimized = false;  // is the application minimized?
bool Maximized = false;  // is the application maximized?
bool Resizing = false;   // are the resize bars being dragged?
POINT LastMousePos;

std::wstring AnsiToWString(const std::string& str)
{
    WCHAR buffer[512];
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, buffer, 512);
    return std::wstring(buffer);
}

class DxException
{
public:
    DxException() = default;
    DxException(HRESULT hr, const std::wstring& functionName, const std::wstring& filename, int lineNumber)
        : ErrorCode(hr), FunctionName(functionName), Filename(filename), LineNumber(lineNumber)
    {
    }

    HRESULT ErrorCode = S_OK;
    std::wstring FunctionName;
    std::wstring Filename;
    int LineNumber = -1;

    std::wstring ToString()const
    {
        // Get the string description of the error code.
        _com_error err(ErrorCode);
        std::wstring msg = err.ErrorMessage();

        return FunctionName + L" failed in " + Filename + L"; line " + std::to_wstring(LineNumber) + L"; error: " + msg;
    }
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

static UINT CalcConstantBufferByteSize(UINT byteSize)
{
    return (byteSize + 255) & ~255;
}

ComPtr<ID3DBlob> CompileShader(
    const std::wstring& filename,
    const D3D_SHADER_MACRO* defines,
    const std::string& entrypoint,
    const std::string& target)
{
    UINT compileFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)  
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    HRESULT hr = S_OK;

    ComPtr<ID3DBlob> byteCode = nullptr;
    ComPtr<ID3DBlob> errors;
    hr = D3DCompileFromFile(filename.c_str(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entrypoint.c_str(), target.c_str(), compileFlags, 0, &byteCode, &errors);

    if (errors != nullptr)
        OutputDebugStringA((char*)errors->GetBufferPointer());

    ThrowIfFailed(hr);

    return byteCode;
}
