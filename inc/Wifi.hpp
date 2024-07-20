#pragma once

#include <stdint.h>
#include <queue>
#include "timer.hpp"

using namespace std;


// 发送消息结构体
typedef struct
{
    uint8_t data[1024];
    int size;
} TxMsgDef;


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

// 发送队列
queue<TxMsgDef> TxQueue;
// 
WifiResponesDef WifiRespones;

