#include <stdio.h>
#include <windows.h>
#include "util.h"

#include <queue>
#include <vector>
#include <map>
#include <string>

#include <conio.h>  // For _kbhit and _getch

#include "StateMachine.h"

#include "EventModel.hpp"
#include "TextUtil.hpp"
#include "timer.hpp"
#include "Wifi.hpp"
#include "AsyncATEvent.hpp"

using namespace std;

// 顶层状态枚举
typedef enum
{
    WifiTopLayerState_off,
    WifiTopLayerState_error,
    WifiTopLayerState_setup,
    WifiTopLayerState_reset,

    WifiTopLayerState_AT_check,
    WifiTopLayerState_AT_entry,
    WifiTopLayerState_AT,
    WifiTopLayerState_AT_exit,

    WifiTopLayerState_TT_check,
    wifiTopLayerState_TT_entry,
    WifiTopLayerState_TT,
    WifiTopLayerState_TT_exit,
} WifiTopLayerState;

typedef enum {
    WifiATState_error,
    WifiATState_reset,
    WifiATState_scanAP,
    WifiATState_getInfo,
    WifiATState_connectAP,
    WifiATState_disconnectAP,
    WifiATState_connectSocket,
    WifiATState_disconnectSocket,
}WifiATState;

// wifi 功能标志
typedef union
{
    struct {
        int reset : 1;
        int scanAP : 1;
        int connectAP : 1;
        int disconnectAP : 1;
        int getIPInfo : 1;
        int connectSocket : 1;
        int disconnectSocket : 1;
    } flag;
    uint64_t notEmpty;
} WifiATFlagsDef;
WifiATFlagsDef wifiATFlags = {0};

typedef struct {
    bool isEN;
    bool isAT_Mode;
    bool isConnectAP;
    bool isConnectSocket;
}WifiFlagsDef;
WifiFlagsDef wifiFlags = {0};


// wifi 响应事件
EventModel WifiEvent;

// 定时器组件
TimerComponent timerComponent;

// wifi 顶层状态机
StateMachine WifiTopLayerSM;
// wifi AT 操作状态机
StateMachine WifiAT_SM;

// 异步 AT 事件模型
AsyncATEvent asyncATEvent;

// AT 操作选择器
bool WifiSmAtSelect(){
    // 扫描AP
    if(wifiATFlags.flag.scanAP){
        wifiATFlags.flag.scanAP = 0;
        // 激活异步AT处理
        asyncATEvent.Init("AT+CWLAP\r\n", [](AsyncATEvent::Event event) {}, 20000, true );
        return true;
    }
    // 获取IP信息   
    if(wifiATFlags.flag.getIPInfo) {
        wifiATFlags.flag.getIPInfo = 0;
        // 激活异步AT处理
        asyncATEvent.Init("AT+CIPSTA?\r\n", [](AsyncATEvent::Event event) {}, 5555, true);
        return true;
    }
    return false;
}


// wifi 状态机初始化
void WifiTopLayerSMInit()
{
    wifiFlags.isEN = true;
    // 错误
    WifiTopLayerSM.AddStateNode(WifiTopLayerState_error, [](StateMachine::StateType beforState) -> StateMachine::StateType
    {
        static bool timeoutFlag = false;
        // 从其他节点跳转而来
        if (WifiTopLayerState_error != beforState) {
            logd("WifiTopLayerState_error from : %d", beforState);

            // 退出 透传模式
            TxMsgDef msg;
            msg.size = sprintf((char*)msg.data, "+++");
            TxQueue.push(msg);

            // 添加一个定时器
            timeoutFlag = false;
            timerComponent.AddTimer(555, []() {
                timeoutFlag = true;
            });
        }
        // 等待完成
        if (timeoutFlag == false) {
            return WifiTopLayerState_error;
        }
        // 前往复位环节
        if (wifiFlags.isEN)
        {
            // 退出 透传模式
            TxMsgDef msg;
            msg.size = sprintf((char*)msg.data, "AT+CIPMODE=0\r\n");
            TxQueue.push(msg);
            return WifiTopLayerState_reset;
        }
        return WifiTopLayerState_error;
    });
    WifiTopLayerSM.LinkState(WifiTopLayerState_error, WifiTopLayerState_error, []() {});
    WifiTopLayerSM.LinkState(WifiTopLayerState_error, WifiTopLayerState_reset, []() {});

    // 复位
    WifiTopLayerSM.AddStateNode(WifiTopLayerState_reset, [](StateMachine::StateType beforState) -> StateMachine::StateType
    {
        static bool timeoutFlag = false;
        // 从其他节点跳转而来
        if (beforState != WifiTopLayerState_reset) {

            logd("wifi reset begin...");

            WifiRespones.Clear();
            TxMsgDef msg;
            msg.size = sprintf((char*)msg.data, "AT+RST\r\n");
            TxQueue.push(msg);
            // 添加一个定时器
            timeoutFlag = false;
            timerComponent.AddTimer(1000, []() {
                timeoutFlag = true;
            });
        }
        if (timeoutFlag == false) {

            return WifiTopLayerState_reset;
        }
        return WifiTopLayerState_setup;
    });
    WifiTopLayerSM.LinkState(WifiTopLayerState_reset, WifiTopLayerState_reset, []() {});
    WifiTopLayerSM.LinkState(WifiTopLayerState_reset, WifiTopLayerState_setup, []() {});

    // 启动
    WifiTopLayerSM.AddStateNode(WifiTopLayerState_setup, [](StateMachine::StateType beforState) -> StateMachine::StateType {
        static bool timeoutFlag = false;
        static int timerHandle = 0;
        // 从其他节点跳转而来
        if (beforState != WifiTopLayerState_setup) {
            logd("wifi setup begin...");
            // 清空响应 必须先于发送指令执行
            WifiRespones.Clear();
            // 发送指令
            TxMsgDef msg;
            msg.size = sprintf((char*)msg.data, "AT\r\n");
            TxQueue.push(msg);
            // 添加一个定时器
            timeoutFlag = false;
            timerHandle = timerComponent.AddTimer(4000, []() {
                timeoutFlag = true;
            });
        }
        // 收到ok响应
        if (WifiRespones.ok) {
            timerComponent.DelTimer(timerHandle);
            return WifiTopLayerState_AT_check;
        }
        else if (WifiRespones.error) // 收到 error 响应
        {
            timerComponent.DelTimer(timerHandle);
            logd("wifi setup error");
            return WifiTopLayerState_error;
        }
        // 等待
        if (timeoutFlag == false) {
            return WifiTopLayerState_setup;
        }

        // 超时
        logd("wifi setup timeout");
        return WifiTopLayerState_error;
    });
    WifiTopLayerSM.LinkState(WifiTopLayerState_setup, WifiTopLayerState_setup, []() {});
    WifiTopLayerSM.LinkState(WifiTopLayerState_setup, WifiTopLayerState_error, []() {});
    WifiTopLayerSM.LinkState(WifiTopLayerState_setup, WifiTopLayerState_AT_check, []() {});

    // 检查是否需要处理AT指令
    WifiTopLayerSM.AddStateNode(WifiTopLayerState_AT_check, [](int befor)->int {
        static bool timeoutFlag = false;
        static int timerHandle = 0;
        if (wifiATFlags.notEmpty) {
            if (wifiFlags.isAT_Mode) {
                // 当前已经是 AT 模式 直接进入AT操作
                return WifiTopLayerState_AT;
            }
            else {
                // 当前为 TT 模式 先退出 TT 模式
                // 退出之前至少等待 200ms
                if(befor != WifiTopLayerState_AT_check) {
                    timeoutFlag = false;
                    timerHandle = timerComponent.AddTimer(200, []() {
                        timeoutFlag = true;
                    });
                }
                if(timeoutFlag == false) {
                    return WifiTopLayerState_AT_check;
                }
                return WifiTopLayerState_TT_exit;
            }
        }
        else {
            // 如果不需要处理AT操作 直接进入 TT 检测
            return WifiTopLayerState_TT_check;
        }
    });
    WifiTopLayerSM.LinkState(WifiTopLayerState_AT_check, WifiTopLayerState_AT_check, []() {});
    WifiTopLayerSM.LinkState(WifiTopLayerState_AT_check, WifiTopLayerState_AT, []() {});
    WifiTopLayerSM.LinkState(WifiTopLayerState_AT_check, WifiTopLayerState_TT_exit, []() {});
    WifiTopLayerSM.LinkState(WifiTopLayerState_AT_check, WifiTopLayerState_TT_check, []() {});

    // 退出透传模式
    WifiTopLayerSM.AddStateNode(WifiTopLayerState_TT_exit, [](int befor)->int {
        static bool timeoutFlag = false;
        static int timerHandle = 0;
        // first entry
        if (befor != WifiTopLayerState_TT_exit) {
            logd("wifi TT exit ...");
            // 退出透传模式
            TxMsgDef msg;
            msg.size = sprintf((char*)msg.data, "+++");
            TxQueue.push(msg);
            // 等待
            timeoutFlag = false;
            timerHandle = timerComponent.AddTimer(300, []() {
                timeoutFlag = true;
            });
        }
        if (timeoutFlag == false)
        {
            return WifiTopLayerState_TT_exit;
        }
        return WifiTopLayerState_AT_entry;
    });
    WifiTopLayerSM.LinkState(WifiTopLayerState_TT_exit, WifiTopLayerState_TT_exit, []() {});
    WifiTopLayerSM.LinkState(WifiTopLayerState_TT_exit, WifiTopLayerState_AT_entry, []() {});


    // 进入AT模式
    WifiTopLayerSM.AddStateNode(WifiTopLayerState_AT_entry, [](int befor)->int {
        static bool timeoutFlag = false;
        static int timerHandle = -1;
        // first entry
        if (befor != WifiTopLayerState_AT_entry) {
            logd("wifi AT entry ...");
            // 清空响应 必须先于发送指令执行
            WifiRespones.Clear();
            // 退出 透传模式
            TxMsgDef msg;
            msg.size = sprintf((char*)msg.data, "AT+CIPMODE=0\r\n");
            TxQueue.push(msg);
            //
            timeoutFlag = false;
            timerHandle = timerComponent.AddTimer(4000, []() {
                timeoutFlag = true;
            });
        }
        // 响应 OK
        if(WifiRespones.ok)
        {
            wifiFlags.isAT_Mode = true;
            return WifiTopLayerState_AT;
        }else if (WifiRespones.error) // 响应 error
        {
            return WifiTopLayerState_error;
        }
        // 超时检测
        if (!timeoutFlag)
        {
            return WifiTopLayerState_AT_entry;
        }
        // 超时
        return WifiTopLayerState_error;
    });
    WifiTopLayerSM.LinkState(WifiTopLayerState_AT_entry, WifiTopLayerState_AT_entry, []() {});
    WifiTopLayerSM.LinkState(WifiTopLayerState_AT_entry, WifiTopLayerState_AT, []() {});
    WifiTopLayerSM.LinkState(WifiTopLayerState_AT_entry, WifiTopLayerState_error, []() {});

    // 执行AT操作
    WifiTopLayerSM.AddStateNode(WifiTopLayerState_AT, [](int befor)->int {
        static bool isRun = false;
        //
        if(befor != WifiTopLayerState_AT) {
            logd("wifi AT exec ...");
            // AT操作选择器
            if(WifiSmAtSelect()) {
                isRun = true;
                return WifiTopLayerState_AT;
            }
            
            // 没有找到指令 跳过 AT 执行过程
            isRun = false;
        } 
        if(isRun) {
            AsyncATEvent::Event e = asyncATEvent.Polling();
            // 事件进行中
            if(e == AsyncATEvent::EventNone) {
                return WifiTopLayerState_AT;
            }
            isRun = false;
            // 完成
            if(e == AsyncATEvent::EventOK) {
                return WifiTopLayerState_TT_check;
            }
            // 错误 | 超时
            // if(e == AsyncATEvent::EventError || e == AsyncATEvent::EventErrorTimeout || e== AsyncATEvent::EventErrorWrite) {
            // }
            if(e == AsyncATEvent::EventErrorTimeout) {
                logd("wifi AT exec timeout");
            }
            return WifiTopLayerState_error;
        }
        return WifiTopLayerState_TT_check;
    });
    WifiTopLayerSM.LinkState(WifiTopLayerState_AT, WifiTopLayerState_AT, []() {});
    WifiTopLayerSM.LinkState(WifiTopLayerState_AT, WifiTopLayerState_TT_check, []() {});
    WifiTopLayerSM.LinkState(WifiTopLayerState_AT, WifiTopLayerState_error, []() {});


    // 检查是否需要执行透传
    WifiTopLayerSM.AddStateNode(WifiTopLayerState_TT_check, [](int befor)->int {
        if(wifiFlags.isConnectAP && wifiFlags.isConnectSocket) {
            // 当前处于AT模式 需要退出AT模式
            if(wifiFlags.isAT_Mode) {
                return WifiTopLayerState_AT_exit;
            }
            // 当前为 TT 模式 直接进入透传
            return WifiTopLayerState_TT;
        }
        return WifiTopLayerState_AT_check;
    });
    WifiTopLayerSM.LinkState(WifiTopLayerState_TT_check, WifiTopLayerState_TT, []() {});
    WifiTopLayerSM.LinkState(WifiTopLayerState_TT_check, WifiTopLayerState_AT_check, []() {});
    WifiTopLayerSM.LinkState(WifiTopLayerState_TT_check, WifiTopLayerState_AT_exit, []() {});

    // 退出 AT 模式
    WifiTopLayerSM.AddStateNode(WifiTopLayerState_AT_exit, [](int befor)->int {
        static bool timeoutFlag;
        static int timerHandle;
        if(befor != WifiTopLayerState_AT_exit) {
            logd("wifi AT exit ...");
            // 清空响应 必须先于发送指令执行
            WifiRespones.Clear();
            // 进入(透传接收模式)
            TxMsgDef msg;
            msg.size = sprintf((char*)msg.data, "AT+CIPMODE=1\r\n");
            TxQueue.push(msg);
            //
            timeoutFlag = false;
            timerHandle = timerComponent.AddTimer(4000, []() {
                timeoutFlag = true;
            });
        }
        // 响应 OK
        if(WifiRespones.ok)
        {
            wifiFlags.isAT_Mode = false;
            return wifiTopLayerState_TT_entry;
        }else if (WifiRespones.error) // 响应 error
        {
            return WifiTopLayerState_error;
        }
        // 超时检测
        if (!timeoutFlag)
        {
            return WifiTopLayerState_AT_exit;
        }
        // 超时
        return WifiTopLayerState_error;
    });
    WifiTopLayerSM.LinkState(WifiTopLayerState_AT_exit, WifiTopLayerState_AT_exit, []() {});
    WifiTopLayerSM.LinkState(WifiTopLayerState_AT_exit, WifiTopLayerState_error, []() {});
    WifiTopLayerSM.LinkState(WifiTopLayerState_AT_exit, wifiTopLayerState_TT_entry, []() {});


    // 切换透传
    WifiTopLayerSM.AddStateNode(wifiTopLayerState_TT_entry, [](int befor)->int {
        static bool timeoutFlag;
        static int timerHandle;
        if(befor != WifiTopLayerState_AT_exit) {
            logd("wifi AT exit ...");
            // 清空响应 必须先于发送指令执行
            WifiRespones.Clear();
            // 进入透传模式
            TxMsgDef msg;
            msg.size = sprintf((char*)msg.data, "AT+CIPSEND\r\n");
            TxQueue.push(msg);
            //
            timeoutFlag = false;
            timerHandle = timerComponent.AddTimer(4000, []() {
                timeoutFlag = true;
            });
        }
        // 响应 OK
        if(WifiRespones.ok)
        {
            wifiFlags.isAT_Mode = false;
            return WifiTopLayerState_TT;
        }else if (WifiRespones.error) // 响应 error
        {
            return WifiTopLayerState_error;
        }
        // 超时检测
        if (!timeoutFlag)
        {
            return wifiTopLayerState_TT_entry;
        }
        // 超时
        return WifiTopLayerState_error;
    });
    WifiTopLayerSM.LinkState(wifiTopLayerState_TT_entry, wifiTopLayerState_TT_entry, []() {});
    WifiTopLayerSM.LinkState(wifiTopLayerState_TT_entry, WifiTopLayerState_error, []() {});
    WifiTopLayerSM.LinkState(wifiTopLayerState_TT_entry, WifiTopLayerState_TT, []() {});
    

    // 进行透传操作
    WifiTopLayerSM.AddStateNode(WifiTopLayerState_TT, [](int befor)->int {
        logd("wifi TT exec ...");
        return  WifiTopLayerState_AT_check;
    });
    WifiTopLayerSM.LinkState(WifiTopLayerState_TT, WifiTopLayerState_AT_check, []() {});


    wifiATFlags.flag.scanAP = 1;



    WifiTopLayerSM.Goto(WifiTopLayerState_error);
}

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
            wifiATFlags.flag.scanAP = 1;
            return;
        }
        if(strncmp(keyBuffer, "2", 1) == 0) {
            wifiATFlags.flag.connectAP = 1;
            return;
        }
        if(strncmp(keyBuffer, "3", 1) == 0) {
            wifiATFlags.flag.connectSocket = 1;
            return;
        }
        if(strncmp(keyBuffer, "4", 1) == 0) {
            wifiATFlags.flag.reset = 1;
            return;
        }
        if(strncmp(keyBuffer, "5", 1) == 0) {
            wifiATFlags.flag.getIPInfo = 1;
            return;
        }
        
    }
}

int main(int argc, const char** argv)
{

    WifiATFlagsDef aa = { 0 };
    logd("%x", aa.notEmpty);
    aa.flag.disconnectAP = 1;
    logd("%x", aa.notEmpty);


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

    // 填充测试指令
    // TxMsgDef msg;
    // msg.size = sprintf((char*)msg.data, "AT\r\n");
    // TxQueue.push(msg);

    // 监听OK指令
    WifiEvent.ListenEvent("OK", [](uint8_t* data, int len) {
        WifiRespones.ok = true;
    });

    // 监听ERROR指令
    WifiEvent.ListenEvent("ERROR", [](uint8_t* data, int len) {
        WifiRespones.error = true;
    });

    // 读取数据
    unsigned char rxBuf[1024];
    int rxBufRemainSize = 0;
    // 残留数据标记
    bool rxBufRemainFlag = false;

    // 添加一个定时器
    timerComponent.AddTimer(1000, []() {
        logd("timeout test");
    });

    // 初始化状态机
    WifiTopLayerSMInit();

    BOOL readState;
    DWORD readSize;
    BOOL writeState;
    DWORD writeSize;

    logd("init over");

    OVERLAPPED wol = { 0 };
    OVERLAPPED rol = { 0 };
    ResetEvent(wol.hEvent);
    ResetEvent(rol.hEvent);

    // LOOP
    while (1)
    {
        //监听键盘输入
        KeyBoradFunc();

        Sleep(100);
        // 定时器组件处理
        timerComponent.TimerLoop();

        // 顶层状态机
        WifiTopLayerSM.RunLoop();

        // 写串口
        // 发送消息队列中的所有数据
        while (TxQueue.empty() == false)
        {
            TxMsgDef& msg = TxQueue.front();
            logd("TX >> %s", msg.data);
            // 异步不需要处理返回值
            WriteFile(hSerial, msg.data, msg.size, &writeSize, &wol);
            TxQueue.pop();
        }

        // 读串口
        readState = ReadFile(hSerial, rxBuf + rxBufRemainSize, 1024, &readSize, &rol);
        //异步不需要处理返回值
        // if (readState == false)
        // {
        //     logd("read com error");
        //     CloseHandle(hSerial);
        //     Sleep(2222);
        //     hSerial = CreateFile("\\\\.\\COM16",
        //         GENERIC_READ | GENERIC_WRITE,
        //         0,
        //         NULL,
        //         OPEN_EXISTING,
        //         FILE_FLAG_OVERLAPPED,
        //         NULL);
        //     if (hSerial == INVALID_HANDLE_VALUE)
        //     {
        //         DWORD e = GetLastError();
        //         logd("serial open error  %x", e);
        //         return -1;
        //     }
        //     continue;
        //     // return -1;
        // }
        if (readSize < 1)
        {
            continue;
        }
        readSize += rxBufRemainSize;
        rxBuf[readSize] = 0;
        logd("RX size: %d", readSize);

        // 检查 是否以换行结尾 判断 是否有不完整数据残留
        if (rxBuf[readSize - 2] == '\r' && rxBuf[readSize - 1] == '\n')
        {
            rxBufRemainFlag = false;
        }
        else
        {
            // 有残留数据
            rxBufRemainFlag = true;
        }

        // 取出每一行
        int lineCount = TextUtil::Split((char*)rxBuf, "\r\n", -1);
        if (rxBufRemainFlag) {
            lineCount -= 1;
        }
        // logd("line count: %d", lineCount);

        // 处理每一行
        char* line = (char*)rxBuf;
        for (size_t i = 0; i < lineCount; i++)
        {
            logd("RX line >> %s", line);

            // 遍历c++ map
            for (auto& it : WifiEvent.EventPool)
            {
                if (TextUtil::WithStart(line, it.first.c_str()))
                {
                    WifiEvent.Exec(it.first.c_str(), (uint8_t*)line, strlen(line));
                }
            }
            // 切换下一行
            line = line + (strlen(line) + 2);
        }

        // 将残留数据拷贝到头部
        // 计算残留数据长度
        rxBufRemainSize = ((char*)rxBuf + readSize) - line;
        if (rxBufRemainFlag)
        {
            logd("RX remain %d", rxBufRemainSize);
            memcpy(rxBuf, line, rxBufRemainSize);
        }
        else {
            rxBufRemainSize = 0;
        }
        logd("RX End ---------------");
    }

    // 关闭串口
    CloseHandle(hSerial);
    return 0;
}