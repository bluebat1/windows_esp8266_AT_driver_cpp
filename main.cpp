#include <stdio.h>
#include <windows.h>
#include "util.h"

#include <queue>
#include <vector>
#include <map>
#include <string>

#include "StateMachine.h"

#include "EventModel.hpp"
#include "TextUtil.hpp"
#include "timer.hpp"

using namespace std;

// 顶层状态枚举
typedef enum
{
    WifiTopLayerState_error,
    WifiTopLayerState_off,
    WifiTopLayerState_setup,
    WifiTopLayerState_reset,

    WifiTopLayerState_AT_check,
    WifiTopLayerState_AT,

    WifiTopLayerState_TT_check,
    WifiTopLayerState_TT,
} WifiTopLayerState;

// 发送消息结构体
typedef struct
{
    uint8_t data[1024];
    int size;
} TxMsgDef;

// 发送队列
queue<TxMsgDef> TxQueue;

// wifi 响应事件
EventModel WifiEvent;

// 定时器组件
TimerComponent timerComponent;

// wifi 顶层状态机
StateMachine WifiTopLayerSM;

// wifi 状态机初始化
void WifiTopLayerSMInit()
{
    // 错误
    WifiTopLayerSM.AddStateNode(WifiTopLayerState_error, [](StateMachine::StateType beforState) -> StateMachine::StateType
    {
        static bool timeoutFlag = false;
        // 从其他节点跳转而来
        if (WifiTopLayerState_error != beforState) {
            // 退出 透传模式
            TxMsgDef msg;
            msg.size = sprintf((char*)msg.data, "+++");
            TxQueue.push(msg);

            // 添加一个定时器
            timerComponent.AddTimer(555, []() {
                timeoutFlag = true;
            });
        }
        // 等待完成
        if (timeoutFlag == false) {
            return WifiTopLayerState_error;
        }
        // 前往复位环节
        return WifiTopLayerState_reset;
    });
    WifiTopLayerSM.LinkState(WifiTopLayerState_error, WifiTopLayerState_error, []() {});
    WifiTopLayerSM.LinkState(WifiTopLayerState_error, WifiTopLayerState_reset, []() {});

    // 复位
    WifiTopLayerSM.AddStateNode(WifiTopLayerState_reset, [](StateMachine::StateType beforState) -> StateMachine::StateType
    {
        static bool timeoutFlag = false;
        // 从其他节点跳转而来
        if (beforState != WifiTopLayerState_reset) {
            TxMsgDef msg;
            msg.size = sprintf((char*)msg.data, "AT+RST\r\n");
            TxQueue.push(msg);
            // 添加一个定时器
            timerComponent.AddTimer(1000, []() {
                timeoutFlag = true;
            });
        }
        if (timeoutFlag == false) {
            return WifiTopLayerState_reset;
        }

        return WifiTopLayerState_setup; });
    WifiTopLayerSM.LinkState(WifiTopLayerState_reset, WifiTopLayerState_reset, []() {});
    WifiTopLayerSM.LinkState(WifiTopLayerState_reset, WifiTopLayerState_setup, []() {});

    // 启动
    WifiTopLayerSM.AddStateNode(WifiTopLayerState_setup, [](StateMachine::StateType beforState) -> StateMachine::StateType {
        static bool timeoutFlag = false;
        // 从其他节点跳转而来
        if (beforState != WifiTopLayerState_setup) {
            TxMsgDef msg;
            msg.size = sprintf((char*)msg.data, "AT\r\n");
            TxQueue.push(msg);
            // 添加一个定时器
            timerComponent.AddTimer(1000, []() {
                timeoutFlag = true;
            });
        }

        if (timeoutFlag == false) {
            return WifiTopLayerState_setup;
        }

        return WifiTopLayerState_setup;
    });
    WifiTopLayerSM.LinkState(WifiTopLayerState_setup, WifiTopLayerState_setup, []() {});
    WifiTopLayerSM.LinkState(WifiTopLayerState_setup, WifiTopLayerState_setup, []() {});

    WifiTopLayerSM.Goto(WifiTopLayerState_error);
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

    // 填充测试指令
    // TxMsgDef msg;
    // msg.size = sprintf((char*)msg.data, "AT\r\n");
    // TxQueue.push(msg);

    // 监听OK指令
    WifiEvent.ListenEvent("OK", [](uint8_t* data, int len) {
        logd("OK");
    });

    // 监听ERROR指令
    WifiEvent.ListenEvent("ERROR", [](uint8_t* data, int len) {
        logd("ERROR");
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
        if (readState == false)
        {
            logd("read com error");
            CloseHandle(hSerial);
            return -1;
        }
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
        logd("line count: %d", lineCount);

        // 处理每一行
        char* line = (char*)rxBuf;
        for (size_t i = 0; i < lineCount; i++)
        {
            logd("line >> %s", line);

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
        logd("RX ---------------");
    }

    //
    CloseHandle(hSerial);
    return 0;
}