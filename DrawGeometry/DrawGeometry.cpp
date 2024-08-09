#include"DrawGeometry.h"
#include <DirectXColors.h>
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
LRESULT CALLBACK //__stdcall
WindowProcess(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

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

std::vector<D3D12_INPUT_ELEMENT_DESC> InputElementDescs;
std::unique_ptr<Mesh> mesh = nullptr;
ComPtr<ID3D12Resource> ObjectConstantBuffer; BYTE* MappedData = nullptr;
UINT IndexCount = 0;

struct Vertex
{
	DirectX::XMFLOAT3 Pos;
	DirectX::XMFLOAT4 Color;
};

struct ObjectConstants
{
	XMFLOAT4X4 WorldViewProj = XMFLOAT4X4(
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f);
};

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
	ThrowIfFailed(d3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
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

	// 重置命令列表
	ThrowIfFailed(commandList->Reset(commandAllocator.Get(), nullptr));

	BuildConstantBuffers();
	BuildRootSignature();
	BuildShadersAndInputLayout();
	BuildGeometry();
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
	//更新mvp矩阵
	/*XMFLOAT4X4 worldViewProj = XMFLOAT4X4(
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f);*/

	XMFLOAT4X4 worldViewProj = XMFLOAT4X4(
		0.7724, 0.0000, 1.6377, 0.0000,
		-1.2602, 1.9715, 0.5944, -0.0000,
		-0.7394, -0.5777, 0.3487, 4.0040,
		-0.7386, -0.5771, 0.3483, 5.0000);

	ObjectConstants objConstants;
	objConstants.WorldViewProj = worldViewProj;

	/* 更新常量缓冲区 */
	//首先获得指向欲更新资源数据的指针
	ThrowIfFailed(ObjectConstantBuffer->Map(0, nullptr, reinterpret_cast<void**>(&MappedData)));

	//利用memcpy函数将数据从系统内存复制到常量缓冲区
	memcpy(MappedData, &objConstants, sizeof(ObjectConstants));
	
	//完成后，依次取消映射，释放映射内存
	if (ObjectConstantBuffer != nullptr)
		ObjectConstantBuffer->Unmap(0, nullptr);

	MappedData = nullptr;

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
	FlushCommandQueue();
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
	std::array<Vertex,8> vertices =
	{
		Vertex({ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::White) }),
		Vertex({ XMFLOAT3(-1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Black) }),
		Vertex({ XMFLOAT3(+1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Red) }),
		Vertex({ XMFLOAT3(+1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::Green) }),
		Vertex({ XMFLOAT3(-1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Blue) }),
		Vertex({ XMFLOAT3(-1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Yellow) }),
		Vertex({ XMFLOAT3(+1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Cyan) }),
		Vertex({ XMFLOAT3(+1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Magenta) })
	};

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);

	mesh = std::make_unique<Mesh>();
	//将顶点数据放入CPU内存中
	ThrowIfFailed(D3DCreateBlob(vbByteSize, &mesh->VertexBufferCPU));
	CopyMemory(mesh->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);
	//将顶点数据从CPU传到GPU中
	mesh->VertexBufferGPU = CreateDefaultBuffer(d3dDevice.Get(),
		commandList.Get(), vertices.data(), vbByteSize, mesh->VertexBufferUploader);

	mesh->VertexByteStride = sizeof(Vertex);
	mesh->VertexBufferByteSize = vbByteSize;

	/*索引部分*/
	std::array<std::uint16_t, 36> indices =
	{
		// front face
		0, 1, 2,
		0, 2, 3,

		// back face
		4, 6, 5,
		4, 7, 6,

		// left face
		4, 5, 1,
		4, 1, 0,

		// right face
		3, 2, 6,
		3, 6, 7,

		// top face
		1, 5, 6,
		1, 6, 2,

		// bottom face
		4, 0, 3,
		4, 3, 7
	};

	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &mesh->IndexBufferCPU));
	CopyMemory(mesh->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	mesh->IndexBufferGPU = CreateDefaultBuffer(d3dDevice.Get(),
		commandList.Get(), indices.data(), ibByteSize, mesh->IndexBufferUploader);

	mesh->IndexFormat = DXGI_FORMAT_R16_UINT;
	mesh->IndexBufferByteSize = ibByteSize;

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
void BuildConstantBuffers()
{
	//创建常量缓冲区，常量缓冲区对大小有要求
	UINT ConstantBufferByteSize = CalcConstantBufferByteSize(sizeof(ObjectConstants));
	UINT elementCount = 1;

	auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto desc = CD3DX12_RESOURCE_DESC::Buffer(ConstantBufferByteSize * elementCount);
	ThrowIfFailed(d3dDevice->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&ObjectConstantBuffer)));
	
	ThrowIfFailed(ObjectConstantBuffer->Map(0, nullptr, reinterpret_cast<void**>(&MappedData)));

	//获得常量缓冲区的起始地址
	D3D12_GPU_VIRTUAL_ADDRESS cbAddress = ObjectConstantBuffer->GetGPUVirtualAddress();

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};    //描述cb子集而不是整个cb
	cbvDesc.BufferLocation = cbAddress;
	cbvDesc.SizeInBytes = CalcConstantBufferByteSize(sizeof(ObjectConstants));

	//创建cb描述符堆
	D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
	cbvHeapDesc.NumDescriptors = 1;
	cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	cbvHeapDesc.NodeMask = 0;

	ThrowIfFailed(d3dDevice->CreateDescriptorHeap(&cbvHeapDesc,
		IID_PPV_ARGS(&CbvHeap)));

	//创建cb视图
	d3dDevice->CreateConstantBufferView(
		&cbvDesc,
		CbvHeap->GetCPUDescriptorHandleForHeapStart());
}
void BuildRootSignature()
{
	//MSDN: 根签名是一个绑定约定，由应用程序定义，着色器使用它来定位他们需要访问的资源。
	
	// 根参数数组，可以存储 表、根描述符或根常量
	CD3DX12_ROOT_PARAMETER slotRootParameter[1];

	// 创建一个只含一个CBV的表.
	CD3DX12_DESCRIPTOR_RANGE cbvTable;
	cbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
	//将表放到根参数数组中
	slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable);

	// 根签名由一组根参数构成。
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(1, slotRootParameter, 0, nullptr,
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
	// 只有当相关的命令列表在GPU上完成执行时，才能重置命令列表分配器;
	// 应用程序应该使用栅栏来确定GPU的执行进度。
	ThrowIfFailed(commandAllocator->Reset());

	// However, when ExecuteCommandList() is called on a particular command 
	// list, that command list can then be reset at any time and must be before 
	// re-recording.
	ThrowIfFailed(commandList->Reset(commandAllocator.Get(), PSO.Get()));

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
	D3D12_VERTEX_BUFFER_VIEW VertexBufferView = mesh->VertexBufferView();
	D3D12_INDEX_BUFFER_VIEW IndexBufferView = mesh->IndexBufferView();

	commandList->IASetVertexBuffers(0, 1, &VertexBufferView);
	commandList->IASetIndexBuffer(&IndexBufferView);
	commandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	commandList->SetGraphicsRootDescriptorTable(0, CbvHeap->GetGPUDescriptorHandleForHeapStart());

	//绘制！
	commandList->DrawIndexedInstanced(IndexCount,1, 0, 0, 0);

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
	/* 处理与鼠标相关的消息 */
	case WM_LBUTTONDOWN:
		MessageBox(0, L"DrawGeometry", L"DrawGeometry", MB_OK);
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
		//OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
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
