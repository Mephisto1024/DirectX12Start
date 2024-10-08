﻿#include"DrawMulGeometry.h"

#include"FrameResource.h"
#include"GeometryGenerator.h"
#include <DirectXColors.h>
#include < WindowsX.h >
#include<vector>
#include<memory>
#include <array>
#include <string>


#if defined(_DEBUG)
#include <dxgidebug.h>
#endif

using namespace Microsoft::WRL;		//ComPtr,
using namespace DirectX;
#pragma comment(lib, "d3d12.lib")    //D3D12GetDebugInterface()
#pragma comment(lib, "dxgi.lib")    //CreateDXGIFactory()
#pragma comment(lib, "d3dcompiler.lib")

bool InitWindow(HINSTANCE instanceHandle, int show);
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
void BuildFrameResources();
void BuildRenderItems();
void BuildDescriptorHeaps();


void PopulateCommandList();
//add for frameresource
const int NumFrameResources = 3;
struct RenderItem
{
	RenderItem() = default;

	// World matrix of the shape that describes the object's local space
	// relative to the world space, which defines the position, orientation,
	// and scale of the object in the world.
	XMFLOAT4X4 World = Identity4x4();

	// Dirty flag indicating the object data has changed and we need to update the constant buffer.
	// Because we have an object cbuffer for each FrameResource, we have to apply the
	// update to each FrameResource.  Thus, when we modify obect data we should set 
	// NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
	int NumFramesDirty = NumFrameResources;

	// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
	UINT ObjCBIndex = -1;

	Mesh* Geo = nullptr;

	// Primitive topology.
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// DrawIndexedInstanced parameters.
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;
};
void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);
/* Win32 */
LRESULT CALLBACK //__stdcall
WindowProcess(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void OnMouseDown(WPARAM btnState, int x, int y);
void OnMouseUp(WPARAM btnState, int x, int y);
void OnMouseMove(WPARAM btnState, int x, int y);

HWND MainWindow = 0;
D3D_FEATURE_LEVEL d3dFeatureLevel = D3D_FEATURE_LEVEL_12_2;
ComPtr<IDXGIFactory7> dxgiFactory;
ComPtr<ID3D12Device9> d3dDevice;
ComPtr<ID3D12Fence1> fence;
ComPtr<ID3D12CommandAllocator> commandAllocator;
ComPtr<ID3D12CommandQueue> commandQueue;
ComPtr<ID3D12GraphicsCommandList> commandList;
ComPtr<IDXGISwapChain4> swapChain;
ComPtr<ID3D12DescriptorHeap> rtvHeap;
ComPtr<ID3D12DescriptorHeap> dsvHeap;
ComPtr<ID3D12DescriptorHeap> CbvHeap;
ComPtr<ID3D12Resource> SwapChainBuffer[SwapChainBufferCount];
ComPtr<ID3D12Resource> depthStencilBuffer;
ComPtr<ID3D12RootSignature> RootSignature = nullptr;
ComPtr<ID3DBlob> VSByteCode = nullptr;
ComPtr<ID3DBlob> PSByteCode = nullptr;
ComPtr<ID3D12PipelineState> PSO = nullptr;

std::vector<std::unique_ptr<RenderItem>> RenderItems;
PassConstants MainPassCBuffer;
std::unordered_map<std::string, std::unique_ptr<Mesh>> Geometries;
// 根据PSO来划分渲染项
std::vector<RenderItem*> OpaqueRenderItems;    // 不透明

XMFLOAT4X4 matModel = Identity4x4();
XMFLOAT4X4 matView = Identity4x4();
XMFLOAT4X4 matProj = Identity4x4();

float mTheta = 1.3f * XM_PI;
float mPhi = XM_PIDIV4;
float mRadius = 5.0f;

std::vector<D3D12_INPUT_ELEMENT_DESC> InputElementDescs;
std::unique_ptr<Mesh> mesh = nullptr;
//ComPtr<ID3D12Resource> ObjectConstantBuffer; BYTE* MappedData = nullptr;
UINT IndexCount = 0;

//add for frameresource
std::vector<std::unique_ptr<FrameResource>> FrameResources;
FrameResource* CurrFrameResource = nullptr;
int CurrFrameResourceIndex = 0;
UINT PassCbvOffset = 0;

//struct ObjectConstants
//{
//	XMFLOAT4X4 WorldViewProj = XMFLOAT4X4(
//		1.0f, 0.0f, 0.0f, 0.0f,
//		0.0f, 1.0f, 0.0f, 0.0f,
//		0.0f, 0.0f, 1.0f, 0.0f,
//		0.0f, 0.0f, 0.0f, 1.0f);
//};


int WINAPI    //__stdcall,参数从右向左压入堆栈
WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
{
	try
	{
		if (!Initialize(hInstance, nShowCmd)) return 0;
		return Run();
	}
	catch (DxException& e)
	{
		
		MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
		return 0;
	}
}
bool InitWindow(HINSTANCE instanceHandle, int show)
{
	/*初始化窗口第一步： 填写WNDCLASS结构体描述窗口的基本属性*/
	WNDCLASS wc = {};
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WindowProcess;    //指向窗口过程函数的指针
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = instanceHandle;
	wc.hIcon = LoadIcon(0, IDI_APPLICATION);
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wc.lpszMenuName = 0;
	wc.lpszClassName = L"Win";

	if (!RegisterClass(&wc))
	{
		MessageBox(0, L"RegisterClass Failed", 0, 0);
		return false;
	}

	MainWindow = CreateWindow(
		wc.lpszClassName,
		L"DrawGeometry",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		width,
		height,
		0, 0, instanceHandle, 0);

	if (MainWindow == 0)
	{
		MessageBox(0, L"CreateWindow Failed", 0, 0);
		return false;
	}

	ShowWindow(MainWindow, show);
	UpdateWindow(MainWindow);
	return true;
}
bool InitDirect3D()
{
#if defined(_DEBUG)    /*
	只有在调试模式下（即 _DEBUG 宏被定义时）才会执行其中的代码。
	*/
	ComPtr<ID3D12Debug> debugController;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
	{
		debugController->EnableDebugLayer();
	}
	//SUCCEEDED 是一个宏，用于检查函数调用是否成功（返回值是否为成功的 HRESULT）
	//调用 D3D12GetDebugInterface 函数来获取 DirectX 12 的调试接口 ID3D12Debug
	//IID_PPV_ARGS是一个宏，用于传递接口的 IID 和指向接口指针的地址。
	// Interface Identifier,
	// Pointer to Pointer to Void,即void**
	//arguments
	// 
	//如果成功获取到调试接口，则调用 EnableDebugLayer 方法启用调试层。
	// 启用调试层后，DirectX 12 会提供更多的运行时调试信息，有助于开发和调试应用程序

#endif

	/*第一步：创建设备*/
	CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory));/*作用： 枚举WARP适配器，创建交换链*/


	HRESULT hardwareResult = D3D12CreateDevice(
		nullptr,
		d3dFeatureLevel,
		IID_PPV_ARGS(&d3dDevice)
	);
	if (FAILED(hardwareResult))
	{
		/*如果调用D3D12CreateDevice失败，程序将回退到WARP设备，windows高级光栅化平台*/
		ComPtr<IDXGIAdapter> warpAdapter;
		ThrowIfFailed(dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));

		ThrowIfFailed(D3D12CreateDevice(
			warpAdapter.Get(),
			d3dFeatureLevel,
			IID_PPV_ARGS(&d3dDevice)
		));
	}
	/*第二步：创建围栏并获取描述符大小*/
	ThrowIfFailed(d3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
	rtvDescriptorSize = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	CbvUavDescriptorSize = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	/*第三步：检测MSAA支持*/
	/*第四步：创建命令队列，命令列表分配器，命令列表*/
	CreateCommandObjects();
	/*第五步：创建交换链*/
	CreateSwapChain();
	/*第六步：创建描述符堆*/
	CreateDescriptorHeaps();
	/*初始化之外*/

	/*第七步：创建RT视图*/

	/*问：为何这里不用D3D12_CPU_DESCRIPTOR_HANDLE类型的句柄？*/
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(rtvHeap->GetCPUDescriptorHandleForHeapStart());
	for (UINT i = 0; i < SwapChainBufferCount; i++)
	{
		//获得交换链内的第i个缓冲区
		ThrowIfFailed(swapChain->GetBuffer(i, IID_PPV_ARGS(&SwapChainBuffer[i])));
		//为此缓冲区创建一个rtv
		d3dDevice->CreateRenderTargetView(SwapChainBuffer[i].Get(), nullptr, rtvHeapHandle);
		//偏移到描述符堆中的下一个缓冲区
		rtvHeapHandle.Offset(1, rtvDescriptorSize);
	}
	/*第八步：创建depth,stencil视图*/
	D3D12_RESOURCE_DESC depthStencilDesc = {};
	depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthStencilDesc.Alignment = 0;
	depthStencilDesc.Width = width;
	depthStencilDesc.Height = height;
	depthStencilDesc.DepthOrArraySize = 1;
	depthStencilDesc.MipLevels = 1;
	depthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;//
	depthStencilDesc.SampleDesc.Count = msaaState ? 4 : 1;
	depthStencilDesc.SampleDesc.Quality = msaaState ? (msaaQuality - 1) : 0;
	depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE optClear = {};
	optClear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	optClear.DepthStencil.Depth = 1.0f;
	optClear.DepthStencil.Stencil = 0;
	/* 此函数根据我们所提供的属性创建一个资源与一个堆，并把该资源提交到这个堆中 */
	CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);    //报错，要求左值
	ThrowIfFailed(d3dDevice->CreateCommittedResource(
		&heapProperties,    //默认堆：GPU可读写，CPU不可访问
		D3D12_HEAP_FLAG_NONE,
		&depthStencilDesc,
		D3D12_RESOURCE_STATE_COMMON,
		&optClear,
		IID_PPV_ARGS(depthStencilBuffer.GetAddressOf())));

	// 为整个资源的0 mip层创建描述符

	d3dDevice->CreateDepthStencilView(
		depthStencilBuffer.Get(),
		nullptr, /* */
		dsvHeap->GetCPUDescriptorHandleForHeapStart());

	// 将资源从初始状态转换为深度缓冲区
	auto resourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(    //报错，要求左值
		depthStencilBuffer.Get(),
		D3D12_RESOURCE_STATE_COMMON,
		D3D12_RESOURCE_STATE_DEPTH_WRITE);

	commandList->ResourceBarrier(1,&resourceBarrier);
	/* 第九步：设置视口 */

	ScreenViewport.TopLeftX = 0;
	ScreenViewport.TopLeftY = 0;
	ScreenViewport.Width = static_cast<float>(width);
	ScreenViewport.Height = static_cast<float>(height);
	ScreenViewport.MinDepth = 0.0f;
	ScreenViewport.MaxDepth = 1.0f;

	commandList->RSSetViewports(1, &ScreenViewport);
	/* 第十步：设置裁剪矩形 */
	ScissorRect = { 0, 0, width, height };

	commandList->RSSetScissorRects(1, &ScissorRect);
	return true;
}
bool Initialize(HINSTANCE instanceHandle, int show)
{
	if (!InitWindow(instanceHandle, show))
		return false;

	if (!InitDirect3D())
		return false;

	OnResize();

	// 重置命令列表
	ThrowIfFailed(commandList->Reset(commandAllocator.Get(), nullptr));

	
	BuildRootSignature();
	BuildShadersAndInputLayout();
	BuildGeometry();
	BuildRenderItems();
	BuildFrameResources();
	BuildDescriptorHeaps();
	BuildConstantBuffers();
	BuildPSO();

	// 执行初始化命令
	ThrowIfFailed(commandList->Close());
	ID3D12CommandList* cmdsLists[] = { commandList.Get() };
	commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// CPU要等待GPU端初始化完成
	FlushCommandQueue();

	return true;
}
int Run()
{
	MSG msg = { 0 };
	while (msg.message != WM_QUIT)
	{
		//如果有消息则进行处理
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		//否则游戏逻辑代码
		else
		{
			Update();
			Draw();
		}
	}
	return (int)msg.wParam;
}
void Update()
{
	/* 更新mvp矩阵 */
	// Convert Spherical to Cartesian coordinates.
	float x = mRadius * sinf(mPhi) * cosf(mTheta);
	float z = mRadius * sinf(mPhi) * sinf(mTheta);
	float y = mRadius * cosf(mPhi);
	// Build the view matrix.
	XMVECTOR pos = XMVectorSet(x, y, z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&matView, view);

	// 循环获取帧资源数组中的元素
	CurrFrameResourceIndex = (CurrFrameResourceIndex + 1) % NumFrameResources;
	CurrFrameResource = FrameResources[CurrFrameResourceIndex].get();

	// 检查GPU是否执行完处理当前帧资源的所有命令
	// 如果没有，就令CPU等待
	if (CurrFrameResource->Fence != 0 && fence->GetCompletedValue() < CurrFrameResource->Fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(fence->SetEventOnCompletion(CurrFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	/* 更新当前的 帧资源 */

	/* 更新物体的cbuffer */
	auto currObjectCB = CurrFrameResource->ObjectCB.get();
	for (auto& e : RenderItems)
	{
		// Only update the cbuffer data if the constants have changed.  
		// This needs to be tracked per frame resource.
		if (e->NumFramesDirty > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&e->World);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));

			currObjectCB->CopyData(e->ObjCBIndex, objConstants);

			// Next FrameResource need to be updated too.
			e->NumFramesDirty--;
		}
	}
	/* 更新帧的cbuffer */
	XMMATRIX matview = XMLoadFloat4x4(&matView);
	XMMATRIX proj = XMLoadFloat4x4(&matProj);

	XMMATRIX viewProj = XMMatrixMultiply(matview, proj);

	XMStoreFloat4x4(&MainPassCBuffer.View, XMMatrixTranspose(matview));
	XMStoreFloat4x4(&MainPassCBuffer.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&MainPassCBuffer.ViewProj, XMMatrixTranspose(viewProj));

	auto currPassCB = CurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, MainPassCBuffer);
	////首先获得指向欲更新资源数据的指针
	//ThrowIfFailed(ObjectConstantBuffer->Map(0, nullptr, reinterpret_cast<void**>(&MappedData)));

	////利用memcpy函数将数据从系统内存复制到常量缓冲区
	//memcpy(MappedData, &objConstants, sizeof(ObjectConstants));
	//
	////完成后，依次取消映射，释放映射内存
	//if (ObjectConstantBuffer != nullptr)
	//	ObjectConstantBuffer->Unmap(0, nullptr);

	//MappedData = nullptr;

}
void Draw()
{
	// Record all the commands we need to render the scene into the command list.
	PopulateCommandList();

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { commandList.Get() };
	commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// 交换后台缓冲区与前台缓冲区
	ThrowIfFailed(swapChain->Present(0, 0));
	currBackBuffer = (currBackBuffer + 1) % SwapChainBufferCount;

	//等待绘制此帧的一系列命令执行完毕
	//FlushCommandQueue();

	// Advance the fence value to mark commands up to this fence point.
	CurrFrameResource->Fence = ++CurrentFence;

	// Add an instruction to the command queue to set a new fence point. 
	// Because we are on the GPU timeline, the new fence point won't be 
	// set until the GPU finishes processing all the commands prior to this Signal().
	commandQueue->Signal(fence.Get(), CurrentFence);
}
void OnResize()
{
	// Flush before changing any resources.
	FlushCommandQueue();

	ThrowIfFailed(commandList->Reset(commandAllocator.Get(), nullptr));

	// Release the previous resources we will be recreating.
	for (int i = 0; i < SwapChainBufferCount; ++i)
		SwapChainBuffer[i].Reset();
	depthStencilBuffer.Reset();

	// Resize the swap chain.
	ThrowIfFailed(swapChain->ResizeBuffers(
		SwapChainBufferCount,
		width, height,
		BackBufferFormat,
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));

	currBackBuffer = 0;

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(rtvHeap->GetCPUDescriptorHandleForHeapStart());
	for (UINT i = 0; i < SwapChainBufferCount; i++)
	{
		ThrowIfFailed(swapChain->GetBuffer(i, IID_PPV_ARGS(&SwapChainBuffer[i])));
		d3dDevice->CreateRenderTargetView(SwapChainBuffer[i].Get(), nullptr, rtvHeapHandle);
		rtvHeapHandle.Offset(1, rtvDescriptorSize);
	}

	// Create the depth/stencil buffer and view.
	D3D12_RESOURCE_DESC depthStencilDesc;
	depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthStencilDesc.Alignment = 0;
	depthStencilDesc.Width = width;
	depthStencilDesc.Height = height;
	depthStencilDesc.DepthOrArraySize = 1;
	depthStencilDesc.MipLevels = 1;
	depthStencilDesc.Format = DepthStencilFormat;
	depthStencilDesc.SampleDesc.Count = msaaState ? 4 : 1;
	depthStencilDesc.SampleDesc.Quality = msaaState ? (msaaQuality - 1) : 0;
	depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE optClear;
	optClear.Format = DepthStencilFormat;
	optClear.DepthStencil.Depth = 1.0f;
	optClear.DepthStencil.Stencil = 0;
	CD3DX12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	ThrowIfFailed(d3dDevice->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&depthStencilDesc,
		D3D12_RESOURCE_STATE_COMMON,
		&optClear,
		IID_PPV_ARGS(depthStencilBuffer.GetAddressOf())));

	// Create descriptor to mip level 0 of entire resource using the format of the resource.
	d3dDevice->CreateDepthStencilView(depthStencilBuffer.Get(), nullptr, dsvHeap->GetCPUDescriptorHandleForHeapStart());

	// Transition the resource from its initial state to be used as a depth buffer.
	auto Barrier = CD3DX12_RESOURCE_BARRIER::Transition(depthStencilBuffer.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	commandList->ResourceBarrier(1, &Barrier);

	// Execute the resize commands.
	ThrowIfFailed(commandList->Close());
	ID3D12CommandList* cmdsLists[] = { commandList.Get() };
	commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until resize is complete.
	FlushCommandQueue();

	// Update the viewport transform to cover the client area.
	ScreenViewport.TopLeftX = 0;
	ScreenViewport.TopLeftY = 0;
	ScreenViewport.Width = static_cast<float>(width);
	ScreenViewport.Height = static_cast<float>(height);
	ScreenViewport.MinDepth = 0.0f;
	ScreenViewport.MaxDepth = 1.0f;

	ScissorRect = { 0, 0, width, height };

	/* 更新纵横比，重新计算投影矩阵 */
	//这里解决了拉伸的问题
	float AspectRatio = static_cast<float>(width) / height;
	XMMATRIX P = XMMatrixPerspectiveFovLH(XM_PI/4.0f, AspectRatio, 1.0f, 1000.0f);
	XMStoreFloat4x4(&matProj, P);
}
void CreateCommandObjects()
{
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	ThrowIfFailed(d3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue)));

	ThrowIfFailed(d3dDevice->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(commandAllocator.GetAddressOf())));

	ThrowIfFailed(d3dDevice->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		commandAllocator.Get(),    // 关联命令分配器
		nullptr,                   // 将流水线状态对象设置为空，因为我们不会发起任何绘制命令
		IID_PPV_ARGS(commandList.GetAddressOf())));

	/*
	* 关闭命令列表，
	因为在第一次引用命令队列时，我们要对它进行重置，
	而在调用重置方法之前，需要将他关闭
	*/
	commandList->Close();
}
void CreateSwapChain()
{
	// Release the previous swapchain we will be recreating.
	swapChain.Reset();

	DXGI_SWAP_CHAIN_DESC sd = {};
	sd.BufferDesc.Width = width;
	sd.BufferDesc.Height = height;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

	sd.SampleDesc.Count = msaaState ? 4 : 1;
	sd.SampleDesc.Quality = msaaState ? (msaaQuality - 1) : 0;

	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = SwapChainBufferCount;
	sd.OutputWindow = MainWindow;
	sd.Windowed = true;
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	ComPtr<IDXGISwapChain> swapChain_temp;
	// Note: Swap chain uses queue to perform flush.
	ThrowIfFailed(dxgiFactory->CreateSwapChain(
		commandQueue.Get(),
		&sd,
		swapChain_temp.GetAddressOf()));
	ThrowIfFailed(swapChain_temp.As(&swapChain));

}
void CreateDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.NumDescriptors = SwapChainBufferCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	ThrowIfFailed(d3dDevice->CreateDescriptorHeap(
		&rtvHeapDesc, IID_PPV_ARGS(rtvHeap.GetAddressOf())));


	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	ThrowIfFailed(d3dDevice->CreateDescriptorHeap(
		&dsvHeapDesc, IID_PPV_ARGS(dsvHeap.GetAddressOf())));
	/*创建描述符堆后如何访问其中的描述符？*/

	/*
	* GetCPUDescriptorHandleForHeapStart函数获得描述符堆中第一个描述符的句柄
	*
	* D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::CurrentBackBufferView()const
	{
		return CD3DX12_CPU_DESCRIPTOR_HANDLE(
			mRtvHeap->GetCPUDescriptorHandleForHeapStart(),
			mCurrBackBuffer,
			mRtvDescriptorSize);
	}

	为了用偏移量找到当前后台缓冲区的RTV描述符，我们必须知道其大小
	*/
}
void FlushCommandQueue()
{
	CurrentFence++;

	// Add an instruction to the command queue to set a new fence point.  Because we 
	// are on the GPU timeline, the new fence point won't be set until the GPU finishes
	// processing all the commands prior to this Signal().
	ThrowIfFailed(commandQueue->Signal(fence.Get(), CurrentFence));
	// Wait until the GPU has completed commands up to this fence point.
	if (fence->GetCompletedValue() < CurrentFence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);

		// Fire event when GPU hits current fence.  
		ThrowIfFailed(fence->SetEventOnCompletion(CurrentFence, eventHandle));

		// Wait until the GPU hits current fence event is fired.
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
}
 
void BuildShadersAndInputLayout()
{
	HRESULT hr = S_OK;
	VSByteCode = CompileShader(L"VertexShader.hlsl", nullptr, "main", "vs_5_0");
	PSByteCode = CompileShader(L"PixelShader.hlsl", nullptr, "main", "ps_5_0");

	InputElementDescs =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
}
void BuildGeometry()
{
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(1.5f, 0.5f, 1.5f, 3);
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(20.0f, 30.0f, 60, 40);
	GeometryGenerator::MeshData sphere = geoGen.CreateSphere(0.5f, 20, 20);
	GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20);

	//
	// We are concatenating all the geometry into one big vertex/index buffer.  So
	// define the regions in the buffer each submesh covers.
	//

	// Cache the vertex offsets to each object in the concatenated vertex buffer.
	UINT boxVertexOffset = 0;
	UINT gridVertexOffset = (UINT)box.Vertices.size();
	UINT sphereVertexOffset = gridVertexOffset + (UINT)grid.Vertices.size();
	UINT cylinderVertexOffset = sphereVertexOffset + (UINT)sphere.Vertices.size();

	// Cache the starting index for each object in the concatenated index buffer.
	UINT boxIndexOffset = 0;
	UINT gridIndexOffset = (UINT)box.Indices32.size();
	UINT sphereIndexOffset = gridIndexOffset + (UINT)grid.Indices32.size();
	UINT cylinderIndexOffset = sphereIndexOffset + (UINT)sphere.Indices32.size();

	// Define the SubmeshGeometry that cover different 
	// regions of the vertex/index buffers.

	SubmeshGeometry boxSubmesh;
	boxSubmesh.IndexCount = (UINT)box.Indices32.size();
	boxSubmesh.StartIndexLocation = boxIndexOffset;
	boxSubmesh.BaseVertexLocation = boxVertexOffset;

	SubmeshGeometry gridSubmesh;
	gridSubmesh.IndexCount = (UINT)grid.Indices32.size();
	gridSubmesh.StartIndexLocation = gridIndexOffset;
	gridSubmesh.BaseVertexLocation = gridVertexOffset;

	SubmeshGeometry sphereSubmesh;
	sphereSubmesh.IndexCount = (UINT)sphere.Indices32.size();
	sphereSubmesh.StartIndexLocation = sphereIndexOffset;
	sphereSubmesh.BaseVertexLocation = sphereVertexOffset;

	SubmeshGeometry cylinderSubmesh;
	cylinderSubmesh.IndexCount = (UINT)cylinder.Indices32.size();
	cylinderSubmesh.StartIndexLocation = cylinderIndexOffset;
	cylinderSubmesh.BaseVertexLocation = cylinderVertexOffset;

	//
	// Extract the vertex elements we are interested in and pack the
	// vertices of all the meshes into one vertex buffer.
	//

	auto totalVertexCount =
		box.Vertices.size() +
		grid.Vertices.size() +
		sphere.Vertices.size() +
		cylinder.Vertices.size();

	std::vector<Vertex> vertices(totalVertexCount);

	UINT k = 0;
	for (size_t i = 0; i < box.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = box.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(DirectX::Colors::DarkGreen);
	}

	for (size_t i = 0; i < grid.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = grid.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(DirectX::Colors::ForestGreen);
	}

	for (size_t i = 0; i < sphere.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = sphere.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(DirectX::Colors::Crimson);
	}

	for (size_t i = 0; i < cylinder.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = cylinder.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(DirectX::Colors::SteelBlue);
	}

	std::vector<std::uint16_t> indices;
	indices.insert(indices.end(), std::begin(box.GetIndices16()), std::end(box.GetIndices16()));
	indices.insert(indices.end(), std::begin(grid.GetIndices16()), std::end(grid.GetIndices16()));
	indices.insert(indices.end(), std::begin(sphere.GetIndices16()), std::end(sphere.GetIndices16()));
	indices.insert(indices.end(), std::begin(cylinder.GetIndices16()), std::end(cylinder.GetIndices16()));

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<Mesh>();
	geo->Name = "shapeGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = CreateDefaultBuffer(d3dDevice.Get(),
		commandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = CreateDefaultBuffer(d3dDevice.Get(),
		commandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	geo->DrawArgs["box"] = boxSubmesh;
	geo->DrawArgs["grid"] = gridSubmesh;
	geo->DrawArgs["sphere"] = sphereSubmesh;
	geo->DrawArgs["cylinder"] = cylinderSubmesh;

	Geometries[geo->Name] = std::move(geo);

	IndexCount = (UINT)indices.size();
}
void BuildPSO()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
	ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	psoDesc.InputLayout = { InputElementDescs.data(), (UINT)InputElementDescs.size() };
	psoDesc.pRootSignature = RootSignature.Get();
	psoDesc.VS =
	{
		reinterpret_cast<BYTE*>(VSByteCode->GetBufferPointer()),
		VSByteCode->GetBufferSize()
	};
	psoDesc.PS =
	{
		reinterpret_cast<BYTE*>(PSByteCode->GetBufferPointer()),
		PSByteCode->GetBufferSize()
	};
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = BackBufferFormat;
	psoDesc.SampleDesc.Count = msaaState ? 4 : 1;
	psoDesc.SampleDesc.Quality = msaaState ? (msaaQuality - 1) : 0;
	psoDesc.DSVFormat = DepthStencilFormat;
	ThrowIfFailed(d3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&PSO)));
}
void BuildFrameResources()
{
	for (int i = 0; i < NumFrameResources; ++i)
	{
		FrameResources.push_back(std::make_unique<FrameResource>(d3dDevice.Get(),
			1, (UINT)RenderItems.size()));
	}
}
void BuildRenderItems()
{
	auto boxRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&boxRitem->World, XMMatrixScaling(2.0f, 2.0f, 2.0f) * XMMatrixTranslation(0.0f, 0.5f, 0.0f));
	boxRitem->ObjCBIndex = 0;
	boxRitem->Geo = Geometries["shapeGeo"].get();
	boxRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
	boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].BaseVertexLocation;
	RenderItems.push_back(std::move(boxRitem));

	auto gridRitem = std::make_unique<RenderItem>();
	gridRitem->World = Identity4x4();
	gridRitem->ObjCBIndex = 1;
	gridRitem->Geo = Geometries["shapeGeo"].get();
	gridRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].IndexCount;
	gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].StartIndexLocation;
	gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].BaseVertexLocation;
	RenderItems.push_back(std::move(gridRitem));

	UINT objCBIndex = 2;
	for (int i = 0; i < 5; ++i)
	{
		auto leftCylRitem = std::make_unique<RenderItem>();
		auto rightCylRitem = std::make_unique<RenderItem>();
		auto leftSphereRitem = std::make_unique<RenderItem>();
		auto rightSphereRitem = std::make_unique<RenderItem>();

		XMMATRIX leftCylWorld = XMMatrixTranslation(-5.0f, 1.5f, -10.0f + i * 5.0f);
		XMMATRIX rightCylWorld = XMMatrixTranslation(+5.0f, 1.5f, -10.0f + i * 5.0f);

		XMMATRIX leftSphereWorld = XMMatrixTranslation(-5.0f, 3.5f, -10.0f + i * 5.0f);
		XMMATRIX rightSphereWorld = XMMatrixTranslation(+5.0f, 3.5f, -10.0f + i * 5.0f);

		XMStoreFloat4x4(&leftCylRitem->World, rightCylWorld);
		leftCylRitem->ObjCBIndex = objCBIndex++;
		leftCylRitem->Geo = Geometries["shapeGeo"].get();
		leftCylRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		leftCylRitem->IndexCount = leftCylRitem->Geo->DrawArgs["cylinder"].IndexCount;
		leftCylRitem->StartIndexLocation = leftCylRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
		leftCylRitem->BaseVertexLocation = leftCylRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation;

		XMStoreFloat4x4(&rightCylRitem->World, leftCylWorld);
		rightCylRitem->ObjCBIndex = objCBIndex++;
		rightCylRitem->Geo = Geometries["shapeGeo"].get();
		rightCylRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		rightCylRitem->IndexCount = rightCylRitem->Geo->DrawArgs["cylinder"].IndexCount;
		rightCylRitem->StartIndexLocation = rightCylRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
		rightCylRitem->BaseVertexLocation = rightCylRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation;

		XMStoreFloat4x4(&leftSphereRitem->World, leftSphereWorld);
		leftSphereRitem->ObjCBIndex = objCBIndex++;
		leftSphereRitem->Geo = Geometries["shapeGeo"].get();
		leftSphereRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		leftSphereRitem->IndexCount = leftSphereRitem->Geo->DrawArgs["sphere"].IndexCount;
		leftSphereRitem->StartIndexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
		leftSphereRitem->BaseVertexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;

		XMStoreFloat4x4(&rightSphereRitem->World, rightSphereWorld);
		rightSphereRitem->ObjCBIndex = objCBIndex++;
		rightSphereRitem->Geo = Geometries["shapeGeo"].get();
		rightSphereRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		rightSphereRitem->IndexCount = rightSphereRitem->Geo->DrawArgs["sphere"].IndexCount;
		rightSphereRitem->StartIndexLocation = rightSphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
		rightSphereRitem->BaseVertexLocation = rightSphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;

		RenderItems.push_back(std::move(leftCylRitem));
		RenderItems.push_back(std::move(rightCylRitem));
		RenderItems.push_back(std::move(leftSphereRitem));
		RenderItems.push_back(std::move(rightSphereRitem));
	}

	// All the render items are opaque.
	for (auto& e : RenderItems)
		OpaqueRenderItems.push_back(e.get());
}
void BuildDescriptorHeaps()
{
	//统计一共要渲染的物体个数
	UINT objCount = (UINT)OpaqueRenderItems.size();

	// 计算要创建的描述符个数，每个帧资源中的每个物体还有pass都需要创建cbv描述符
	UINT numDescriptors = (objCount + 1) * NumFrameResources;

	// 计算pass cbv的起始偏移量
	PassCbvOffset = objCount * NumFrameResources;

	D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
	cbvHeapDesc.NumDescriptors = numDescriptors;
	cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	cbvHeapDesc.NodeMask = 0;
	ThrowIfFailed(d3dDevice->CreateDescriptorHeap(&cbvHeapDesc,
		IID_PPV_ARGS(&CbvHeap)));
}

void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
	UINT objCBByteSize = CalcConstantBufferByteSize(sizeof(ObjectConstants));

	auto objectCB = CurrFrameResource->ObjectCB->Resource();

	// For each render item...
	for (size_t i = 0; i < ritems.size(); ++i)
	{
		auto ri = ritems[i];
		D3D12_VERTEX_BUFFER_VIEW VertexBufferView = ri->Geo->VertexBufferView();
		D3D12_INDEX_BUFFER_VIEW IndexBufferView = ri->Geo->IndexBufferView();
		cmdList->IASetVertexBuffers(0, 1,&VertexBufferView);
		cmdList->IASetIndexBuffer(&IndexBufferView);
		cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		// Offset to the CBV in the descriptor heap for this object and for this frame resource.
		UINT cbvIndex = CurrFrameResourceIndex * (UINT)OpaqueRenderItems.size() + ri->ObjCBIndex;
		auto cbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(CbvHeap->GetGPUDescriptorHandleForHeapStart());
		cbvHandle.Offset(cbvIndex, CbvUavDescriptorSize);

		cmdList->SetGraphicsRootDescriptorTable(0, cbvHandle);

		cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}

void BuildConstantBuffers()
{
	UINT objCBByteSize = CalcConstantBufferByteSize(sizeof(ObjectConstants));

	UINT objCount = (UINT)OpaqueRenderItems.size();

	// Need a CBV descriptor for each object for each frame resource.
	for (int frameIndex = 0; frameIndex < NumFrameResources; ++frameIndex)
	{
		auto objectCB = FrameResources[frameIndex]->ObjectCB->Resource();
		for (UINT i = 0; i < objCount; ++i)
		{
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress = objectCB->GetGPUVirtualAddress();

			// Offset to the ith object constant buffer in the buffer.
			cbAddress += i * objCBByteSize;

			// Offset to the object cbv in the descriptor heap.
			int heapIndex = frameIndex * objCount + i;
			auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(CbvHeap->GetCPUDescriptorHandleForHeapStart());
			handle.Offset(heapIndex, CbvUavDescriptorSize);

			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
			cbvDesc.BufferLocation = cbAddress;
			cbvDesc.SizeInBytes = objCBByteSize;

			d3dDevice->CreateConstantBufferView(&cbvDesc, handle);
		}
	}

	UINT passCBByteSize = CalcConstantBufferByteSize(sizeof(PassConstants));

	// Last three descriptors are the pass CBVs for each frame resource.
	for (int frameIndex = 0; frameIndex < NumFrameResources; ++frameIndex)
	{
		auto passCB = FrameResources[frameIndex]->PassCB->Resource();
		D3D12_GPU_VIRTUAL_ADDRESS cbAddress = passCB->GetGPUVirtualAddress();

		// Offset to the pass cbv in the descriptor heap.
		int heapIndex = PassCbvOffset + frameIndex;
		auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(CbvHeap->GetCPUDescriptorHandleForHeapStart());
		handle.Offset(heapIndex, CbvUavDescriptorSize);

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
		cbvDesc.BufferLocation = cbAddress;
		cbvDesc.SizeInBytes = passCBByteSize;

		d3dDevice->CreateConstantBufferView(&cbvDesc, handle);
	}
}
void BuildRootSignature()
{
	//MSDN: 根签名是一个绑定约定，由应用程序定义，着色器使用它来定位他们需要访问的资源。
	
	// 根参数数组，可以存储 表、根描述符或根常量
	CD3DX12_ROOT_PARAMETER slotRootParameter[2];

	// 创建一个只含一个CBV的表.(常量缓冲区视图)
	CD3DX12_DESCRIPTOR_RANGE cbvTable0;
	cbvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
	CD3DX12_DESCRIPTOR_RANGE cbvTable1;
	cbvTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);
	//将表放到根参数数组中
	slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable0);
	slotRootParameter[1].InitAsDescriptorTable(1, &cbvTable1);
	// 根签名由一组根参数构成。
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(2, slotRootParameter, 0, nullptr,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	
	// dx12规定必须先将根签名的描述布局进行序列化处理，
	// 待其转换为ID3DBlob后才可传入CreateRootSignature函数正式创建根签名
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);
	// 创建根签名
	ThrowIfFailed(d3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(&RootSignature)));
}
void PopulateCommandList()
{	
	auto cmdListAlloc = CurrFrameResource->CmdListAlloc;
	// 只有当相关的命令列表在GPU上完成执行时，才能重置命令列表分配器;
	// 应用程序应该使用栅栏来确定GPU的执行进度。
	ThrowIfFailed(cmdListAlloc->Reset());

	// However, when ExecuteCommandList() is called on a particular command 
	// list, that command list can then be reset at any time and must be before 
	// re-recording.
	ThrowIfFailed(commandList->Reset(cmdListAlloc.Get(), PSO.Get()));

	commandList->RSSetViewports(1, &ScreenViewport);
	commandList->RSSetScissorRects(1, &ScissorRect);

	// Indicate a state transition on the resource usage.
	auto Barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		SwapChainBuffer[currBackBuffer].Get(),
		D3D12_RESOURCE_STATE_PRESENT, 
		D3D12_RESOURCE_STATE_RENDER_TARGET);
	commandList->ResourceBarrier(1, &Barrier);

	//get CurrentBackBufferView
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView = CD3DX12_CPU_DESCRIPTOR_HANDLE(
		rtvHeap->GetCPUDescriptorHandleForHeapStart(),
		currBackBuffer,
		rtvDescriptorSize);
	//get dsView
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView = dsvHeap->GetCPUDescriptorHandleForHeapStart();

	// Clear the back buffer and depth buffer.
	commandList->ClearRenderTargetView(CurrentBackBufferView, Colors::AntiqueWhite, 0, nullptr);
	commandList->ClearDepthStencilView(DepthStencilView, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// Specify the buffers we are going to render to.
	commandList->OMSetRenderTargets(1, &CurrentBackBufferView, true, &DepthStencilView);

	ID3D12DescriptorHeap* descriptorHeaps[] = { CbvHeap.Get() };
	commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	commandList->SetGraphicsRootSignature(RootSignature.Get());

	//get VertexBufferView and IndexBufferView
	int passCbvIndex = PassCbvOffset + CurrFrameResourceIndex;
	auto passCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(CbvHeap->GetGPUDescriptorHandleForHeapStart());
	passCbvHandle.Offset(passCbvIndex, CbvUavDescriptorSize);
	commandList->SetGraphicsRootDescriptorTable(1, passCbvHandle);

	//绘制！
	DrawRenderItems(commandList.Get(), OpaqueRenderItems);

	// Indicate a state transition on the resource usage.
	auto Barrier1 = CD3DX12_RESOURCE_BARRIER::Transition(
		SwapChainBuffer[currBackBuffer].Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PRESENT);
	commandList->ResourceBarrier(1, &Barrier1);

	// Done recording commands.
	ThrowIfFailed(commandList->Close());

}
LRESULT CALLBACK WindowProcess(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_ACTIVATE:
		if (LOWORD(wParam) == WA_INACTIVE)
		{
			Paused = true;
			//mTimer.Stop();
		}
		else
		{
			Paused = false;
			//mTimer.Start();
		}
		return 0;

	/* 用户调整窗口大小时便会产生此消息 */
	case WM_SIZE:
		// Save the new client area dimensions.
		width = LOWORD(lParam);
		height = HIWORD(lParam);
		if (d3dDevice)
		{
			if (wParam == SIZE_MINIMIZED)
			{
				Paused = true;
				Minimized = true;
				Maximized = false;
			}
			else if (wParam == SIZE_MAXIMIZED)
			{
				Paused = false;
				Minimized = false;
				Maximized = true;
				OnResize();
			}
			else if (wParam == SIZE_RESTORED)
			{

				// Restoring from minimized state?
				if (Minimized)
				{
					Paused = false;
					Minimized = false;
					OnResize();
				}

				// Restoring from maximized state?
				else if (Maximized)
				{
					Paused = false;
					Maximized = false;
					OnResize();
				}
				else if (Resizing)
				{
					// If user is dragging the resize bars, we do not resize 
					// the buffers here because as the user continuously 
					// drags the resize bars, a stream of WM_SIZE messages are
					// sent to the window, and it would be pointless (and slow)
					// to resize for each WM_SIZE message received from dragging
					// the resize bars.  So instead, we reset after the user is 
					// done resizing the window and releases the resize bars, which 
					// sends a WM_EXITSIZEMOVE message.
				}
				else // API call such as SetWindowPos or mSwapChain->SetFullscreenState.
				{
					OnResize();
				}
			}
		}
		return 0;

		// WM_EXITSIZEMOVE is sent when the user grabs the resize bars.
	case WM_ENTERSIZEMOVE:
		Paused = true;
		Resizing = true;
		return 0;

		// WM_EXITSIZEMOVE is sent when the user releases the resize bars.
		// Here we reset everything based on the new window dimensions.
	case WM_EXITSIZEMOVE:
		Paused = false;
		Resizing = false;
		OnResize();
		return 0;

	/* 处理与鼠标相关的消息 */
	case WM_LBUTTONDOWN:
		//MessageBox(0, L"DrawGeometry", L"DrawGeometry", MB_OK);

		return 0;
	
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
		//OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
		//OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_MOUSEMOVE:
		OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;

	case WM_KEYDOWN:
		if (wParam == VK_ESCAPE)
			DestroyWindow(MainWindow);
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

void OnMouseDown(WPARAM btnState, int x, int y)
{
	LastMousePos.x = x;
	LastMousePos.y = y;

	SetCapture(MainWindow);    //捕获当前窗口的鼠标
}

void OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();    //释放鼠标捕获
}

void OnMouseMove(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_LBUTTON) != 0)
	{
		// Make each pixel correspond to a quarter of a degree.
		float dx = XMConvertToRadians(1.0f * static_cast<float>(x - LastMousePos.x));
		float dy = XMConvertToRadians(1.0f * static_cast<float>(y - LastMousePos.y));

		// Update angles based on input to orbit camera around box.
		mTheta -= dx;
		mPhi -= dy;

		// Restrict the angle mPhi.
		mPhi = Clamp<float>(mPhi, 0.1f, XM_PI - 0.1f);
	}
	//else if ((btnState & MK_RBUTTON) != 0)
	//{
	//	// Make each pixel correspond to 0.005 unit in the scene.
	//	float dx = 0.005f * static_cast<float>(x - mLastMousePos.x);
	//	float dy = 0.005f * static_cast<float>(y - mLastMousePos.y);

	//	// Update the camera radius based on input.
	//	mRadius += dx - dy;

	//	// Restrict the radius.
	//	mRadius = MathHelper::Clamp(mRadius, 3.0f, 15.0f);
	//}

	LastMousePos.x = x;
	LastMousePos.y = y;
}
