#pragma once

#include <stdint.h>
#include <queue>
#include "timer.hpp"

using namespace std;

// 外部生成的定时器组件
extern TimerComponent timerComponent;

// 发送消息结构体
typedef struct
{
    uint8_t data[1024];
    int size;
} TxMsgDef;

queue<TxMsgDef> TxQueue;

// WiFi 响应指令操作
class WifiResponesDef {
public:
    bool ok;
    bool error;
    WifiResponesDef() {
        ok = false;
        error = false;
    };
    ~WifiResponesDef() {};
    void Clear() {
        ok = false;
        error = false;
    }
};
WifiResponesDef WifiRespones;

