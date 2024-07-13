#define WIN32_LEAN_AND_MEAN
#include<windows.h>
#include"InitializeDirectX12.h"
#include <d3d12.h>	/*包含函数：
D3D12CreateDevice()
D3D12GetDebugInterface()
类：
ID3D12Device4
*/
#include "d3dx12.h"/*包含 创建RT视图所需的句柄 CD3DX12_CPU_DESCRIPTOR_HANDLE */
#include <wrl.h>	/*windows Runtime C++ Template Library，方便使用ComPtr*/

#include<dxgi1_6.h>/*包含：
IDXGIFactory7

*/

#if defined(_DEBUG)
#include <dxgidebug.h>
#endif


using namespace Microsoft::WRL;		//ComPtr,

#pragma comment(lib, "d3d12.lib")    //D3D12GetDebugInterface()
#pragma comment(lib, "dxgi.lib")    //CreateDXGIFactory()

bool InitWindow(HINSTANCE instanceHandle, int show);
bool InitDirect3D();
bool Initialize(HINSTANCE instanceHandle, int show);
int Run();
void Draw();
void CreateCommandObjects();
void CreateSwapChain();
void CreateDescriptorHeaps();
LRESULT CALLBACK //__stdcall
WindowProcess(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

HWND MainWindow = 0;
D3D_FEATURE_LEVEL d3dFeatureLevel =  D3D_FEATURE_LEVEL_12_2;
ComPtr<IDXGIFactory7> dxgiFactory;
ComPtr<ID3D12Device9> d3dDevice;
ComPtr<ID3D12Fence1> fence;
ComPtr<ID3D12CommandAllocator> commandAllocator;
ComPtr<ID3D12CommandQueue> commandQueue;
ComPtr<ID3D12GraphicsCommandList> commandList;
ComPtr<IDXGISwapChain4> swapChain;
ComPtr<ID3D12DescriptorHeap> rtvHeap;
ComPtr<ID3D12DescriptorHeap> dsvHeap;
ComPtr<ID3D12Resource> SwapChainBuffer[SwapChainBufferCount];
ComPtr<ID3D12Resource> depthStencilBuffer;

int WINAPI    //__stdcall,参数从右向左压入堆栈
WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
{
	try
	{
		if (!Initialize(hInstance, nShowCmd)) return 0;
		return Run();
	}
	catch(DxException& e)
	{
		MessageBox(nullptr, (LPCWSTR)e.Error(), L"HR Failed", MB_OK);
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
		L"InitializeDirectX12",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
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
	ThrowIfFailed(d3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)));
	rtvDescriptorSize = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
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
	return true;
}
bool Initialize(HINSTANCE instanceHandle, int show)
{
	if (!InitWindow(instanceHandle,show))
		return false;

	if (!InitDirect3D())
		return false;

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
			Draw();
		}
	}
	return (int)msg.wParam;
}
void Draw()
{
	
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
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NumDescriptors = SwapChainBufferCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	ThrowIfFailed(d3dDevice->CreateDescriptorHeap(
		&rtvHeapDesc, IID_PPV_ARGS(rtvHeap.GetAddressOf())));


	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
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
LRESULT CALLBACK WindowProcess(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_LBUTTONDOWN:
		MessageBox(0, L"InitializeDirectX12", L"Initialize", MB_OK);
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
