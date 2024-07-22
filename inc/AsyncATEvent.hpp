#pragma once

#include "timer.hpp"
#include <string>

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

    void Init(const char* cmd, EventHook hook, int timeout, bool checkTimeout);
    Event Polling();

private:
    string _cmd;
    EventHook _hook;
    bool _writed = false;
    int timeoutHandle = 0;
    int _timeout = 4000;
    bool _checkTimeout = true;

};
