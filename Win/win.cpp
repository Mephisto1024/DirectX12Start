#include<windows.h>



HWND MainWindow = 0;
/* 初始化失败则返回false */
bool InitWindow(HINSTANCE instanceHandle, int show);
int Run();

LRESULT CALLBACK //
WindowProcess(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

int APIENTRY    //参数从右向左压入堆栈
WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
{
	if (!InitWindow(hInstance, nShowCmd)) return 0;
	return Run();
}
bool InitWindow(HINSTANCE instanceHandle, int show)
{
	/*初始化窗口第一步： 填写WNDCLASS结构体描述窗口的基本属性*/
	WNDCLASS wndclass;
	wndclass.style = CS_HREDRAW | CS_VREDRAW;
	wndclass.lpfnWndProc = WindowProcess;    //指向窗口过程函数的指针
	wndclass.cbClsExtra = 0;
	wndclass.cbWndExtra = 0;
	wndclass.hInstance = instanceHandle;
	wndclass.hIcon = LoadIcon(0, IDI_APPLICATION);
	wndclass.hCursor = LoadCursor(0, IDC_ARROW);
	wndclass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wndclass.lpszMenuName = 0;
	wndclass.lpszClassName = L"Win";

	if (!RegisterClass(&wndclass))
	{
		MessageBox(0, L"RegisterClass Failed", 0, 0);
		return false;
	}

	MainWindow = CreateWindow(
		wndclass.lpszClassName,
		L"Win32",
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
int Run()
{
	MSG msg = { 0 };
	BOOL bRet = 1;
	while ((bRet = GetMessage(&msg, 0, 0, 0)) != 0)
	{
		if (bRet == -1)
		{
			MessageBox(0, L"GetMessage Failed", L"Error", MB_OK);
			break;
		}
		else
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	return (int)msg.wParam;
}
LRESULT CALLBACK WindowProcess(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_LBUTTONDOWN:
		MessageBox(0, L"HelloWorld", L"Hello", MB_OK);
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
