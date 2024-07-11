#include <stdio.h>
#include <windows.h>

#include <queue>
#include <vector>
#include <map>
#include <string>


#include "EventModel.hpp"
#include "TextUtil.hpp"

#define logd(fmt, ...) printf(fmt "\r\n", ##__VA_ARGS__)

using namespace std;



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






int main(int argc, const char **argv)
{
    // 当端口号 大于 9时 要用 \\\\.\\COMx
    HANDLE hSerial = CreateFile("\\\\.\\COM16",
                                GENERIC_READ | GENERIC_WRITE,
                                0,
                                NULL,
                                OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL,
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
    TimeOuts.ReadIntervalTimeout = 1000;
    TimeOuts.ReadTotalTimeoutConstant = 5000;
    TimeOuts.ReadTotalTimeoutMultiplier = 500;

    // 设定写超时
    TimeOuts.WriteTotalTimeoutConstant = 2000;
    TimeOuts.WriteTotalTimeoutMultiplier = 500;

    SetCommTimeouts(hSerial, &TimeOuts);

    // 清空缓冲区
    PurgeComm(hSerial, PURGE_TXCLEAR | PURGE_RXCLEAR);
    Sleep(500);

    // 填充测试指令
    TxMsgDef msg;
    msg.size = sprintf((char *)msg.data, "AT\r\n");
    TxQueue.push(msg);

    // 监听OK指令
    WifiEvent.ListenEvent("OK", [](uint8_t *data, int len)
                          { logd("OK"); });

    // 监听ERROR指令
    WifiEvent.ListenEvent("ERROR", [](uint8_t *data, int len)
                          { logd("ERROR"); });

    // 读取数据
    unsigned char rxBuf[1024];
    int rxBufIndex = 0;
    // 残留数据标记
    bool rxBufRemainFlag = false;

    BOOL readState;
    DWORD readSize;
    BOOL writeState;
    DWORD writeSize;
    logd("init over");
    // LOOP
    while (1)
    {
        Sleep(111);
        logd("loop");
        // 写串口
        // 发送消息队列中的所有数据
        while (TxQueue.empty() == false)
        {
            TxMsgDef &msg = TxQueue.front();
            logd("TX >> %s", msg.data);
            writeState = WriteFile(hSerial, msg.data, msg.size, &writeSize, NULL);
            if (writeState == false)
            {
                logd("write com error");
                CloseHandle(hSerial);
                return -1;
            }
            TxQueue.pop();
        }

        // 读串口
        readState = ReadFile(hSerial, rxBuf + rxBufIndex, 1024, &readSize, NULL);
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
        rxBuf[readSize] = 0;
        logd("RX >> %s", rxBuf);
        logd("RX size: %d", readSize);

        // 取出每一行
        int lineCount = TextUtil::Split((char *)rxBuf, "\r\n", -1);
        // 检查 是否以换行结尾 判断 是否有不完整数据残留
        // if (rxBuf[readSize - 2] == '\r' && rxBuf[readSize - 1] == '\n')
        // {
        //     rxBufRemainFlag = true;
        // }
        // else
        // {
        //     // 有残留数据
        //     rxBufRemainFlag = true;
        //     // 不处理未完整的行
        //     lineCount -= 1;
        // }

        logd("line count: %d", lineCount);

        // 处理每一行
        char *line = (char *)rxBuf;
        for (size_t i = 0; i < lineCount; i++)
        {
            logd("line >> %s", line);

            // 遍历c++ map
            for (auto &it : WifiEvent.EventPool)
            {
                if (TextUtil::WithStart(line, it.first.c_str()))
                {
                    WifiEvent.Exec(it.first.c_str(), (uint8_t *)line, strlen(line));
                }
            }
            // 切换下一行
            // line = TextUtil::ToNextSubStr(line);
            line = line + (strlen(line)+2);
        }

        // 将残留数据拷贝到头部
        // 计算残留数据长度
        int rxBufRemainSize = ((char *)rxBuf+readSize) - line;
        logd("RX remain %d", rxBufRemainSize);
        if( rxBufRemainSize > 0 ) {
            memcpy(rxBuf, line, rxBufRemainSize);
            rxBufIndex = rxBufRemainSize;
        }
    }

    //
    CloseHandle(hSerial);
    return 0;
}