#pragma once

#include <stdint.h>
#include <queue>
#include <windows.h>

#include "StateMachine.h"
#include "EventModel.hpp"
#include "AsyncATEvent.hpp"

#include "timer.hpp"
#include "TextUtil.hpp"

using namespace std;

class Wifi
{
private:
    /* data */
public:

    // 发送消息结构体
    typedef struct
    {
        uint8_t data[1024];
        int size;
    } TxMsgDef;


    // WiFi 响应指令操作
    class ResponesDef {
    public:
        bool ok;
        bool error;
        ResponesDef() {
            ok = false;
            error = false;
        };
        ~ResponesDef() {};
        void Clear() {
            ok = false;
            error = false;
        }
    };


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
    } TopLayerState;

    // AT状态
    typedef enum {
        WifiATState_error,
        WifiATState_reset,
        WifiATState_scanAP,
        WifiATState_getInfo,
        WifiATState_connectAP,
        WifiATState_disconnectAP,
        WifiATState_connectSocket,
        WifiATState_disconnectSocket,
    }ATState;

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
    } ATFlagsDef;

    typedef struct {
        bool isEN;
        bool isRun;
        bool isAT_Mode;
        bool isScanApOver;
        bool isConnectAP;
        bool isConnectSocket;
    }FlagsDef;

    typedef struct {
        string ssid;
        string password;
        string mac;
        string ip;
    }ConectApInfoDef;

    // wifi 连接信息
    ConectApInfoDef ConectApInfo;

    // 驱动状态
    FlagsDef Flags = {0};
    // AT操作标志
    ATFlagsDef ATFlags = {0};
    
    // wifi 响应事件
    EventModel Event;

    // wifi 顶层状态机
    StateMachine TopLayerSM;
    // wifi AT 操作状态机
    StateMachine AT_SM;

    // 异步 AT 事件模型
    AsyncATEvent asyncATEvent;
    

    // 发送队列
    queue<TxMsgDef> TxQueue;
    // 响应器 
    ResponesDef Respones;

    // 定时器组件
    TimerComponent timerComponent;

    // 串口句柄
    HANDLE ttys;

    
    // 构造函数
    Wifi(/* args */);

    // 析构函数
    ~Wifi(); 

    // 获取单例
    static Wifi & Instance();
    
    // AT操作选择器
    bool WifiSmAtSelect();

    // 顶层状态机初始化
    void TopLayerSMInit();

    // 写串口
    int write(uint8_t * data, int len);
    // 读串口
    int read(uint8_t * buffer, int len);

    // 循环处理
    bool Loop();

};











