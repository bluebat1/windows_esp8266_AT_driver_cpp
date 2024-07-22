#include <stdio.h>
#include <windows.h>
#include "util.h"

#include <queue>
#include <vector>
#include <map>
#include <string>

#include <conio.h>  // For _kbhit and _getch

#include "Wifi.hpp"
#include "TextUtil.hpp"

#include "test.hpp"


using namespace std;



// 处理键盘输入
char keyBuffer[1024] = {0};
int keyBufferIndex = 0;
void KeyBoradFunc(){
    if (_kbhit()) {
        char key = _getch();
        keyBuffer[keyBufferIndex++] = key;
        keyBuffer[keyBufferIndex] = 0;
        // Get the character
        if(key == VK_ESCAPE){
            exit(0);
            return;
        }
        if(key != VK_RETURN) {
            return;
        }
        keyBufferIndex = 0;
        
        if(TextUtil::WithStart(keyBuffer, "?")){
            logd("please input number of function:");
            logd("1.ScanAP");
            logd("2.ConnectAP");
            logd("3.ConnectSocket");
            logd("4.Reset");
            logd("5.GetIPInfo");
            return;
        }
        if(strncmp(keyBuffer, "1", 1) == 0) {
            Wifi::Instance().ATFlags.flag.scanAP = 1;
            return;
        }
        if(strncmp(keyBuffer, "2", 1) == 0) {
            Wifi::Instance().ATFlags.flag.connectAP = 1;
            return;
        }
        if(strncmp(keyBuffer, "3", 1) == 0) {
            Wifi::Instance().ATFlags.flag.connectSocket = 1;
            return;
        }
        if(strncmp(keyBuffer, "4", 1) == 0) {
            Wifi::Instance().ATFlags.flag.reset = 1;
            return;
        }
        if(strncmp(keyBuffer, "5", 1) == 0) {
            Wifi::Instance().ATFlags.flag.getIPInfo = 1;
            return;
        }
        
    }
}

int main(int argc, const char** argv)
{


    // 当端口号 大于 9时 要用 \\\\.\\COMx
    HANDLE hSerial = CreateFile("\\\\.\\COM16",
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED,
        NULL);
    if (hSerial == INVALID_HANDLE_VALUE)
    {
        DWORD e = GetLastError();
        logd("serial open error  %x", e);
        return -1;
    }

    // 2.初始化串口
    DCB dcb;
    GetCommState(hSerial, &dcb);
    dcb.BaudRate = 115200;     // 波特率
    dcb.ByteSize = 8;          // 数据位
    dcb.Parity = NOPARITY;     // 奇偶校验
    dcb.StopBits = ONESTOPBIT; // 停止位
    SetCommState(hSerial, &dcb);

    SetupComm(hSerial, 1024, 1024);
    COMMTIMEOUTS TimeOuts;

    // 设定读超时
    TimeOuts.ReadIntervalTimeout = MAXDWORD;
    TimeOuts.ReadTotalTimeoutConstant = 0;
    TimeOuts.ReadTotalTimeoutMultiplier = 0;

    // 设定写超时
    TimeOuts.WriteTotalTimeoutConstant = 2000;
    TimeOuts.WriteTotalTimeoutMultiplier = 50;

    SetCommTimeouts(hSerial, &TimeOuts);

    // 清空缓冲区
    PurgeComm(hSerial, PURGE_TXCLEAR | PURGE_RXCLEAR);
    Sleep(500);

    // 获取控制句柄
    Wifi::Instance().ttys = hSerial;

    logd("init over");
    
    // LOOP
    while (1)
    {
        Sleep(100);

        //监听键盘输入
        KeyBoradFunc();

        // Wifi 任务处理
        Wifi::Instance().Loop();

        // 测试程序
        Test1();
    }

    // 关闭串口
    CloseHandle(hSerial);
    return 0;
}