#define POINTBUF 28
#define TRN 0xFFFFFF
#define WIDTH 1920
#define HEIGHT 1080
#define MUTEX_NAME TEXT("Mutex Object")
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <tchar.h>
#include <vector>

typedef struct
{
    int X;
    int Y;
} Point;

typedef struct
{
    Point p;
} ThreadParam;


typedef struct
{
    ThreadParam params[10];
    int head;
    int tail;
} Queue;

using namespace std;
HWND hWnd;
HDC hdc;
RAWINPUTDEVICE device;
vector<char> rawInputMessageData;
HANDLE hMutex = CreateMutex(NULL, FALSE, MUTEX_NAME);
Point Points[POINTBUF];
Queue q;
int centerOfX;
int centerOfY;
int pointCol;
//マウスのポーリングレート
int hz = 1000;
//Pointが消えるまでの時間(ms)
int sleepTime = 650;
//Pointの重なりを管理するための配列
char pointTable[WIDTH][HEIGHT];

DWORD WINAPI RemovePoint(LPVOID lParam);
void DrawPoint(Point p, int col);
void RedrawPoints(int X, int Y);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
int readConfig();

void enq(Queue* q, ThreadParam param)
{
    q->params[q->tail] = param;
    q->tail++;
    if (q->tail == 10)
    {
        q->tail = 0;
    }
}

ThreadParam deq(Queue* q)
{
    ThreadParam param = q->params[q->head];
    q->head++;
    if (q->head == 10)
    {
        q->head = 0;
    }
    return param;
}

void init(HWND hWnd)
{
    RECT rc;
    GetClientRect(hWnd, &rc);
    centerOfX = rc.right / 2;
    centerOfY = rc.bottom / 2;

    for (int i = 0; i < WIDTH; i++)
    {
        for (int j = 0; j < HEIGHT; j++)
        {
            pointTable[i][j] = 0;
        }
    }
    readConfig();
}

int readConfig() {
    int R, G, B;
    char keys[10][10] = { "R", "G","B","Hz" };
    FILE* fp = NULL;
    char line[256];
    fopen_s(&fp, "config.txt", "r");
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
    pointCol = RGB(R, G, B);
    fclose(fp);
    return 0;
}

auto pointController = [] {
    //Pointsの要素を指定するためのイテレータ
    int i = 0;
    return [=](int X, int Y) mutable -> void {
        if (i == POINTBUF) i = 0;

        RedrawPoints(X, Y);
        ThreadParam param;
        param.p = Points[i];
        enq(&q, param);
        CreateThread(NULL, 0, RemovePoint, NULL, 0, NULL);

        Points[i] = { X, Y };
        DrawPoint(Points[i], pointCol);
        i++;
    };
}();

DWORD WINAPI RemovePoint(LPVOID lParam)
{
    ThreadParam param = deq(&q);
    Point p = param.p;

    Sleep(sleepTime);
    WaitForSingleObject(hMutex, INFINITE);
    for (int i = -1; i < 2; i++)
    {
        for (int j = -1; j < 2; j++)
        {

            int X = i + p.X;
            int Y = j + p.Y;

            if (pointTable[X][Y] > 0)
            {
                pointTable[X][Y]--;
            }

            if (pointTable[X][Y] == 0)
            {
                SetPixelV(hdc, X, Y, TRN);
            }
        }
    }
    ReleaseMutex(hMutex);
    return TRUE;
}

void DrawPoint(Point p, int col)
{
    WaitForSingleObject(hMutex, INFINITE);
    for (int i = -1; i < 2; i++)
    {
        for (int j = -1; j < 2; j++)
        {
            int X = i + p.X;
            int Y = j + p.Y;

            if (col != TRN)
            {
                pointTable[X][Y]++;
                SetPixelV(hdc, X, Y, col);
                continue;
            }

            if (pointTable[X][Y] > 0)
            {
                pointTable[X][Y]--;
            }
            if (pointTable[X][Y] == 0)
            {
                SetPixelV(hdc, X, Y, col);
            }
        }
    }
    ReleaseMutex(hMutex);
}

void RedrawPoints(int X, int Y)
{
    for (int i = 0; i < POINTBUF; i++)
    {
        if (Points[i].X == 0) continue;
        DrawPoint(Points[i], TRN);
        Points[i].X += X - centerOfX;
        Points[i].Y += Y - centerOfY;
        DrawPoint(Points[i], pointCol);
    }
}

//スキップしたRawInputのノルムを保存
int bufX,bufY = 0;
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

        //描画を125hzに固定する
        i++;
        if (i != 8 * hz / 1000)
        {
            bufX += mouseData.lLastX;
            bufY += mouseData.lLastY;
            return;
        }

        //縦方向のブレを強調したいためXを2で除している
        int X = (mouseData.lLastX + bufX) * 1 / 2 * -1;
        int Y = (mouseData.lLastY + bufY) * -1;
        //描画毎のノルムを制限する
        int max = 10;
        if (X > max) X = max;
        if (X < -max) X = -max;
        if (Y > max) Y = max;
        if (Y < -max) Y = -max;

        pointController(centerOfX + X, centerOfY + Y);
        i = 0;
        bufX = 0;
        bufY = 0;
    }
    return;
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
        init(hWnd);
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
    TCHAR szClassName[] = _T("MouseTracer");
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