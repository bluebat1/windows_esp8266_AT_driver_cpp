#pragma once

#include "timer.hpp"
#include "Wifi.hpp"

using namespace std;

// 1、这个事件模型支持轮询，等待由外部实现
class AsyncATEvent
{
private:
public:
    // 事件回传状态
    typedef enum {
        EventNone,
        EventError,
        EventErrorWrite,
        EventErrorTimeout,
        EventOK,
    }Event;

    typedef void(*EventHook)(Event event);

    AsyncATEvent() {
        _hook = NULL;
    };
    ~AsyncATEvent() {};

    void Init(const char* cmd, EventHook hook);
    Event Polling();

private:
    string _cmd;
    EventHook _hook;
    bool _writed = false;
    int timeoutHandle = 0;

};

// 初始化事件模型
// cmd AT指令
// hook 结果回调函数
void AsyncATEvent::Init(const char* cmd, EventHook hook)
{
    _cmd = cmd;
    _hook = hook;
    _writed = false;
}

// 轮询处理事件
AsyncATEvent::Event AsyncATEvent::Polling()
{
    static bool timeoutFlag;
    // 1、发送指令
    if (!_writed) {
        TxMsgDef msg;
        WifiRespones.Clear();
        msg.size = sprintf((char*)msg.data, "%s", _cmd.c_str());
        TxQueue.push(msg);
        _writed = true;
        timeoutFlag = false;
        timeoutHandle = timerComponent.AddTimer(4000, []() {
            timeoutFlag = true;
        });
    }

    // 2、等待响应
    // 响应 OK
    if (WifiRespones.ok)    
    {
        if (_hook) {
            _hook(EventOK);
        }
        timerComponent.DelTimer(timeoutHandle);
        return EventOK;
    }
    else if (WifiRespones.error) // 响应 error
    {
        if (_hook) {
            _hook(EventError);
        }
        timerComponent.DelTimer(timeoutHandle);
        return EventError;
    }
    if (!timeoutFlag) {
        return EventNone;
    }
    // 超时
    if (_hook) {
        _hook(EventErrorTimeout);
    }
    return EventErrorTimeout;
}

