#include "AsyncATEvent.hpp"
#include "Wifi.hpp"

// 初始化事件模型
// cmd AT指令
// hook 结果回调函数
// timeout 超时时间
// checkTimeout 是否检查超时,为false时，polling超时后依然返回EventOK
void AsyncATEvent::Init(const char* cmd, EventHook hook, int timeout, bool checkTimeout)
{
    _cmd = cmd;
    _hook = hook;
    _writed = false;
    _timeout = timeout;
    _checkTimeout = checkTimeout;
}

// 轮询处理事件
AsyncATEvent::Event AsyncATEvent::Polling()
{
    static bool timeoutFlag;
    // 1、发送指令
    if (!_writed) {
        Wifi::TxMsgDef msg;
        Wifi::Instance().Respones.Clear();
        msg.size = sprintf((char*)msg.data, "%s", _cmd.c_str());
        Wifi::Instance().TxQueue.push(msg);
        _writed = true;
        timeoutFlag = false;
        timeoutHandle = Wifi::Instance().timerComponent.AddTimer(_timeout, []() {
            timeoutFlag = true;
        });
    }

    // 2、等待响应
    // 响应 OK
    if (Wifi::Instance().Respones.ok)    
    {
        if (_hook) {
            _hook(EventOK);
        }
        Wifi::Instance().timerComponent.DelTimer(timeoutHandle);
        return EventOK;
    }
    else if (Wifi::Instance().Respones.error) // 响应 error
    {
        if (_hook) {
            _hook(EventError);
        }
        Wifi::Instance().timerComponent.DelTimer(timeoutHandle);
        return EventError;
    }
    if (!timeoutFlag) {
        return EventNone;
    }
    // 超时
    if (_hook) {
        _hook(EventErrorTimeout);
    }
    // 是否检查超时
    if(_checkTimeout) {
        return EventErrorTimeout;
    }
    // 不检查超时
    return EventOK;
}

