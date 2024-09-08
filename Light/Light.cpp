// Light.cpp : 定义应用程序的入口点。
//

#include "framework.h"
#include "Light.h"

#if defined(_DEBUG)
#include <dxgidebug.h>
#endif

#define MAX_LOADSTRING 100



// 全局变量:
HINSTANCE hInst;                                // 当前实例
WCHAR szTitle[MAX_LOADSTRING];                  // 标题栏文本
WCHAR szWindowClass[MAX_LOADSTRING];            // 主窗口类名

// 此代码模块中包含的函数的前向声明:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    // 初始化全局字符串
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_LIGHT, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);
    
    try
    {
        if (!InitInstance(hInstance, nCmdShow)) return 0;
        return Run();
    }
    catch (DxException& e)
    {

        MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
        return 0;
    }
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_LIGHT));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_LIGHT);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // 将实例句柄存储在全局变量中

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}
bool InitDirect3D()
{
#if defined(_DEBUG)
    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
    {
        debugController->EnableDebugLayer();
    }
#endif
    /*第一步：创建设备*/
    CreateDXGIFactory(IID_PPV_ARGS(&DxgiFactory));/*作用： 枚举WARP适配器，创建交换链*/


    HRESULT hardwareResult = D3D12CreateDevice(
        nullptr,
        FeatureLevel,
        IID_PPV_ARGS(&Device)
    );
    if (FAILED(hardwareResult))
    {
        /*如果调用D3D12CreateDevice失败，程序将回退到WARP设备，windows高级光栅化平台*/
        ComPtr<IDXGIAdapter> warpAdapter;
        ThrowIfFailed(DxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));

        ThrowIfFailed(D3D12CreateDevice(
            warpAdapter.Get(),
            FeatureLevel,
            IID_PPV_ARGS(&Device)
        ));
    }
    /*第二步：创建围栏并获取描述符大小*/
    ThrowIfFailed(Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&Fence)));
    rtvDescriptorSize = Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    CbvSrvUavDescriptorSize = Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
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
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(RtvHeap->GetCPUDescriptorHandleForHeapStart());
    for (UINT i = 0; i < SwapChainBufferCount; i++)
    {
        //获得交换链内的第i个缓冲区
        ThrowIfFailed(SwapChain->GetBuffer(i, IID_PPV_ARGS(&SwapChainBuffer[i])));
        //为此缓冲区创建一个rtv
        Device->CreateRenderTargetView(SwapChainBuffer[i].Get(), nullptr, rtvHeapHandle);
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
    CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(Device->CreateCommittedResource(
        &heapProperties,    //默认堆：GPU可读写，CPU不可访问
        D3D12_HEAP_FLAG_NONE,
        &depthStencilDesc,
        D3D12_RESOURCE_STATE_COMMON,
        &optClear,
        IID_PPV_ARGS(DepthStencilBuffer.GetAddressOf())));

    // 为整个资源的0 mip层创建描述符

    Device->CreateDepthStencilView(
        DepthStencilBuffer.Get(),
        nullptr, /* */
        DsvHeap->GetCPUDescriptorHandleForHeapStart());

    // 将资源从初始状态转换为深度缓冲区
    auto resourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(    //报错，要求左值
        DepthStencilBuffer.Get(),
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_DEPTH_WRITE);

    CommandList->ResourceBarrier(1, &resourceBarrier);
    /* 第九步：设置视口 */

    ScreenViewport.TopLeftX = 0;
    ScreenViewport.TopLeftY = 0;
    ScreenViewport.Width = static_cast<float>(width);
    ScreenViewport.Height = static_cast<float>(height);
    ScreenViewport.MinDepth = 0.0f;
    ScreenViewport.MaxDepth = 1.0f;

    CommandList->RSSetViewports(1, &ScreenViewport);
    /* 第十步：设置裁剪矩形 */
    ScissorRect = { 0, 0, width, height };

    CommandList->RSSetScissorRects(1, &ScissorRect);
    return true;
}
bool Initialize(HINSTANCE instanceHandle, int show)
{
    if (!InitInstance(instanceHandle, show))
        return false;

    if (!InitDirect3D())
        return false;

    OnResize();

    // 重置命令列表
    ThrowIfFailed(CommandList->Reset(CommandAllocator.Get(), nullptr));

    BuildConstantBuffers();
    BuildRootSignature();
    BuildShadersAndInputLayout();
    BuildGeometry();
    BuildPSO();

    // 执行初始化命令
    ThrowIfFailed(CommandList->Close());
    ID3D12CommandList* cmdsLists[] = { CommandList.Get() };
    CommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // CPU要等待GPU端初始化完成
    FlushCommandQueue();

    return true;
}
int Run()
{
    MSG msg = { 0 };

    // 主消息循环:
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

}
void Draw()
{

}
void OnResize()
{
}
void CreateCommandObjects()
{
}
void CreateSwapChain()
{
}
void CreateDescriptorHeaps()
{
}
void FlushCommandQueue()
{
}
void BuildConstantBuffers()
{
}
void BuildRootSignature()
{
}
void BuildShadersAndInputLayout()
{
}
void BuildGeometry()
{
}
void BuildPSO()
{
}
void PopulateCommandList()
{
}
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // 分析菜单选择:
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
