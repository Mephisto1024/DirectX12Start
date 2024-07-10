#include<windows.h>
HWND MainWindow = 0;
/* ��ʼ��ʧ���򷵻�false */
bool InitWindow(HINSTANCE instanceHandle, int show);
int Run();

LRESULT CALLBACK //
WindowProcess(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

int WINAPI    //������������ѹ���ջ
WinMain(HINSTANCE hInstance, HINSTANCE hPreInstance, PSTR pCmdLine, int nCmdShow)
{
	if (!InitWindow(hInstance, nCmdShow)) return 0;
	return Run();
}

bool InitWindow(HINSTANCE instanceHandle, int show)
{
	/*��ʼ�����ڵ�һ���� ��дWNDCLASS�ṹ���������ڵĻ�������*/
	WNDCLASS wndclass;
	wndclass.style = CS_HREDRAW | CS_VREDRAW;
	wndclass.lpfnWndProc = WindowProcess;    //ָ�򴰿ڹ��̺�����ָ��
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
		0,0,instanceHandle,0,);

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
	while (bRet = GetMessage(&msg, 0, 0, 0) != 0)
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
	}
	return LRESULT();
}
