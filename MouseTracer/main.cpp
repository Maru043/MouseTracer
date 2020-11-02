#define D3D_DEBUG_INFO	// Direct3Dデバックフラグ
#define DOTBUF 28
#define TRN 0xFFFFFF
#define WIDTH 1920
#define HEIGHT 1080
#define MUTEX_NAME TEXT("Mutex Object")
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <vector>
#include<string>

using namespace std;
int centerOfX;
int centerOfY;
int dotCol;
int hz = 1000;
int sleepTime = 1000;
char table[WIDTH][HEIGHT];

typedef struct
{
    int X;
    int Y;
} Dot;

typedef struct _ThreadParam
{
    Dot d;
    int col;
} ThreadParam;

Dot Dots[DOTBUF];
typedef struct
{
    ThreadParam tps[10];
    int head;
    int tail;
} Queue;

void DrawPoint(Dot d, int col);
void RedrawPoints(int X, int Y);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
DWORD WINAPI ThreadRemovePoint(LPVOID lParam);
HANDLE hMutex = CreateMutex(NULL, FALSE, MUTEX_NAME);
int readConfig();
TCHAR szClassName[] = _T("MouseTracer");

HWND hWnd;
HDC hdc;
RAWINPUTDEVICE device;
vector<char> rawInputMessageData;
Queue q;
char keys[10][10] = {"R", "G","B","Hz"};

void enq(Queue* q, ThreadParam tp)
{
    q->tps[q->tail] = tp;
    q->tail++;
    if (q->tail == 10)
    {
        q->tail = 0;
    }
}

ThreadParam deq(Queue* q)
{
    ThreadParam param = q->tps[q->head];
    q->head++;
    if (q->head == 10)
    {
        q->head = 0;
    }
    return param;
}

void init(int w, int h)
{
    centerOfX = (w) / 2;
    centerOfY = (h) / 2;
    for (int i = 0; i < WIDTH; i++)
    {
        for (int j = 0; j < HEIGHT; j++)
        {
            table[i][j] = 0;
        }
    }
    readConfig();
}

int readConfig() {
    int R, G, B;
    FILE* fp = NULL;
    char line[256];
    fopen_s(&fp,"config.txt","r");
    if (fp == NULL) return -1;
    while (fgets(line, 256, fp) != NULL)
    {
        char key[10];
        int value;
        if (line[0] == '#') continue;
        for (int i = 0; sizeof(line) / sizeof(line[0]); i++) {
            if (line[i] == '=') 
            { 
                line[i] = ' '; 
                break;
            }
        }
        sscanf(line, "%s %d", &key, &value);
        switch (key[0]) {
        case 'R':
            R = value;
            break;
        case 'G':
            G = value;
            break;
        case 'B':
            B = value;
            break;
        case 'H':
            hz = value;
        case 'T':
            sleepTime = value;
        }
    }
    dotCol = RGB(R, G, B);
    fclose(fp);
    return 0;
}

auto DotController = [] {
    int i = 0;
    bool flag = false;
    return [=](int X, int Y) mutable -> void {
        if (i == DOTBUF)
        {
            i = 0;
            flag = true;
        }
        RedrawPoints(X, Y);

        ThreadParam param;
        param.d = Dots[i];
        param.col = TRN;
        enq(&q, param);
        CreateThread(NULL, 0, ThreadRemovePoint, LPVOID(NULL), 0, NULL);

        Dots[i] = { X, Y };
        DrawPoint(Dots[i], dotCol);
        i++;
    };
}();

DWORD WINAPI ThreadRemovePoint(LPVOID lParam)
{
    ThreadParam param = deq(&q);
    Dot d = param.d;
    int col = param.col;

    Sleep(sleepTime);
    WaitForSingleObject(hMutex, INFINITE);
    for (int i = -1; i < 2; i++)
    {
        for (int j = -1; j < 2; j++)
        {

            int X = i + d.X;
            int Y = j + d.Y;

            if (X < 0)
                X = 0;
            if (X > WIDTH - 1)
                X = WIDTH - 1;
            if (Y < 0)
                Y = 0;
            if (Y > HEIGHT - 1)
                Y = HEIGHT - 1;

            if (table[X][Y] > 0)
            {
                table[X][Y]--;
            }

            if (table[X][Y] == 0)
            {
                SetPixelV(hdc, X, Y, TRN);
            }
        }
    }
    ReleaseMutex(hMutex);
    return TRUE;
}

void DrawPoint(Dot d, int col)
{
    WaitForSingleObject(hMutex, INFINITE);
    for (int i = -1; i < 2; i++)
    {
        for (int j = -1; j < 2; j++)
        {
            int X = i + d.X;
            int Y = j + d.Y;

            if (X < 0)
                X = 0;
            if (X > WIDTH - 1)
                X = WIDTH - 1;
            if (Y < 0)
                Y = 0;
            if (Y > HEIGHT - 1)
                Y = HEIGHT - 1;

            if (col != TRN)
            {
                table[X][Y]++;
                SetPixelV(hdc, X, Y, col);
                continue;
            }

            if (table[X][Y] > 0)
            {
                table[X][Y]--;
            }
            if (table[X][Y] == 0)
            {

                SetPixelV(hdc, X, Y, col);
            }
        }
    }
    ReleaseMutex(hMutex);
}

void RedrawPoints(int X, int Y)
{
    for (int i = 0; i < DOTBUF; i++)
    {
        if (Dots[i].X == 0) continue;
        DrawPoint(Dots[i], TRN);
        Dots[i].X += X - centerOfX;
        Dots[i].Y += Y - centerOfY;
        DrawPoint(Dots[i], dotCol);
    }
}

int bufX = 0;
int bufY = 0;
char i = 0;
void OnRawInput(HRAWINPUT hRawInput)
{
    UINT dataSize;

    GetRawInputData(hRawInput, RID_INPUT, NULL, &dataSize, sizeof(RAWINPUTHEADER));
    if (dataSize == 0)
    {
        return;
    }

    if (dataSize > rawInputMessageData.size())
    {
        rawInputMessageData.resize(dataSize);
    }

    void* dataBuf = &rawInputMessageData[0];
    GetRawInputData(hRawInput, RID_INPUT, dataBuf, &dataSize, sizeof(RAWINPUTHEADER));

    const RAWINPUT* raw = (const RAWINPUT*)dataBuf;
    if (raw->header.dwType == RIM_TYPEMOUSE)
    {
        HANDLE deviceHandle = raw->header.hDevice;
        const RAWMOUSE& mouseData = raw->data.mouse;
        i++;
        if (i != 8 * hz / 1000)
        {
            bufX += mouseData.lLastX;
            bufY += mouseData.lLastY;
            return;
        }
        int X = (mouseData.lLastX + bufX) * 1 / 2 * -1;
        int Y = (mouseData.lLastY + bufY) * 1 / 1 * -1;
        int d = 10;
        if (X > d) X = d;
        if (X < -d) X = -d;
        if (Y > d) Y = d;
        if (Y < -d) Y = -d;
     
        DotController(centerOfX + X, centerOfY + Y);
        i = 0;
        bufX = 0;
        bufY = 0;
    }
    return;
}

static VOID funcSetClientSize(HWND hWnd, LONG sx, LONG sy)
{
    RECT rc1;
    RECT rc2;

    GetWindowRect(hWnd, &rc1);
    GetClientRect(hWnd, &rc2);
    sx += ((rc1.right - rc1.left) - (rc2.right - rc2.left));
    sy += ((rc1.bottom - rc1.top) - (rc2.bottom - rc2.top));
    SetWindowPos(hWnd, NULL, 0, 0, sx, sy, (SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOMOVE));
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_INPUT:
        OnRawInput(HRAWINPUT(lParam));
        return (DefWindowProc(hWnd, msg, wParam, lParam));
    case WM_CREATE:
        device.usUsagePage = 0x01;
        device.usUsage = 0x02;
        device.dwFlags = RIDEV_INPUTSINK; //バックグラウンドでもWM_INPUTを受け取れるようにする
        device.hwndTarget = hWnd;         //dwFlagsがRIDEV_INPUTSINKの場合は0にしてはいけない
        RegisterRawInputDevices(&device, 1, sizeof device);
        SetLayeredWindowAttributes(hWnd, TRN, 0, LWA_COLORKEY);
        funcSetClientSize(hWnd, WIDTH, HEIGHT);
        RECT rc;
        GetClientRect(hWnd, &rc);
        init(rc.right, rc.bottom);
        hdc = GetDC(hWnd);
        break;
    case WM_DESTROY:
        device.usUsagePage = 0x01;
        device.usUsage = 0x02;
        device.dwFlags = RIDEV_REMOVE;
        device.hwndTarget = 0;
        ReleaseDC(hWnd, hdc);
        RegisterRawInputDevices(&device, 1, sizeof device);
        PostQuitMessage(0);
        break;
    default:
        return (DefWindowProc(hWnd, msg, wParam, lParam));
    }
    return (0L);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPreInst, LPSTR lpszCmdLine, int nCmdShow)
{
    MSG msg;
    WNDCLASS myProg;
    if (!hPreInst)
    {
        myProg.style = CS_HREDRAW | CS_VREDRAW;
        myProg.lpfnWndProc = WndProc;
        myProg.cbClsExtra = 0;
        myProg.cbWndExtra = 0;
        myProg.hInstance = hInstance;
        myProg.hIcon = NULL;
        myProg.hCursor = LoadCursor(NULL, IDC_ARROW);
        myProg.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        myProg.lpszMenuName = NULL;
        myProg.lpszClassName = szClassName;
        if (!RegisterClass(&myProg))
        {
            return FALSE;
        }
    }
    hWnd = CreateWindowEx(
        WS_EX_LAYERED,
        szClassName,         // class名
        _T("MouseTracer"),   // タイトル
        WS_OVERLAPPEDWINDOW, // Style
        0,                   // X
        0,                   // Y
        WIDTH,               // Width
        HEIGHT,              // Height
        NULL,                // 親ウィンドウまたはオーナーウィンドウのハンドル
        NULL,                // メニューハンドルまたは子ウィンドウ ID
        hInstance,           // アプリケーションインスタンスのハンドル
        NULL                 // ウィンドウ作成データ
    );

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (msg.wParam);
}
