#include "Wifi.hpp"



using namespace std;

// 外部生成的定时器组件
extern TimerComponent timerComponent;

// wifi 实例
static Wifi self;

// 获取 WIFI 单例
Wifi& Wifi::Instance() {
    return self;
}

// 构造函数
Wifi::Wifi() {

    // 监听OK指令
    Event.ListenEvent("OK", [](uint8_t* data, int len) {
        self.Respones.ok = true;
    });

    // 监听ERROR指令
    Event.ListenEvent("ERROR", [](uint8_t* data, int len) {
        self.Respones.error = true;
    });

    // 监听 AP 连接成功
    Event.ListenEvent("WIFI CONNECTED", [](uint8_t* data, int len) {
        self.Flags.isConnectAP = true;
    });

    // 监听 获取IP成功
    Event.ListenEvent("WIFI GOT IP", [](uint8_t* data, int len) {
        self.Flags.isGotIP = true;
    });

    // 监听 获取ip info回传
    Event.ListenEvent("+CIPSTA", [](uint8_t* data, int len) {
        // 找到内容开头
        char* s;
        s = strchr((char*)data, '"');
        if (s == NULL) {
            return;
        }
        s = s + 1;

        // 去除末尾
        char* s2;
        s2 = strchr(s, '"');
        if (s2 == NULL) {
            return;
        }
        *s2 = 0;
        // ip
        if (TextUtil::WithStart((char*)data, "+CIPSTA:ip")) {
            self.ConectApInfo.ip = s;
            return;
        }
        // gateway
        if (TextUtil::WithStart((char*)data, "+CIPSTA:gateway")) {
            self.ConectApInfo.gateway = s;
            return;
        }
        // mask
        if (TextUtil::WithStart((char*)data, "+CIPSTA:mask")) {
            self.ConectApInfo.netmask = s;
            return;
        }

        self.Flags.isGotIP = true;
    });


    // 初始化状态机
    TopLayerSMInit();

    logd("Wifi Init OK.");
}

// 析构函数
Wifi::~Wifi() {
    logd("Wifi Destory");
}

// AT 操作选择器
bool Wifi::WifiSmAtSelect() {
    // 复位
    if (ATFlags.flag.reset) {
        ATFlags.flag.reset = 0;
        TopLayerSM.Goto(WifiTopLayerState_reset);
    }
    // 扫描AP
    if (ATFlags.flag.scanAP) {
        ATFlags.flag.scanAP = 0;
        // 激活异步AT处理
        asyncATEvent.Init("AT+CWLAP\r\n", [](AsyncATEvent::Event event) {
            if (event == AsyncATEvent::EventOK) {
                self.Flags.isScanApOver = true;
            }
        }, 20000, true);
        return true;
    }
    // 连接AP
    if (ATFlags.flag.connectAP) {
        ATFlags.flag.connectAP = 0;
        char info[128];
        snprintf(info, sizeof(info) - 1, "AT+CWJAP=\"%s\",\"%s\"\r\n", ConectApInfo.ssid.c_str(), ConectApInfo.password.c_str());
        // 激活异步AT处理
        asyncATEvent.Init(info, [](AsyncATEvent::Event event) {
            if (event == AsyncATEvent::EventOK) {

            }
        }, 20000, true);
        return true;
    }
    // 获取IP信息   
    if (ATFlags.flag.getIPInfo) {
        ATFlags.flag.getIPInfo = 0;
        // 激活异步AT处理
        asyncATEvent.Init("AT+CIPSTA?\r\n", [](AsyncATEvent::Event event) {
            if (event == AsyncATEvent::EventOK) {
                logd("get ip info OK");
            }
        }, 2222, false);
        return true;
    }

    // 连接 socket
    if (ATFlags.flag.connectSocket) {
        ATFlags.flag.connectSocket = 0;
        char info[128];
        snprintf(info, sizeof(info) - 1, "AT+CIPSTART=\"%s\",\"%s\",%s\r\n", SocketInfo.type.c_str(), SocketInfo.host.c_str(), SocketInfo.port.c_str());
        asyncATEvent.Init(info, [](AsyncATEvent::Event event) {
            if (event == AsyncATEvent::EventOK) {
                logd("connect socket OK");
                self.Flags.isSocketConnected = true;
            }
        }, 5555, true);
        return true;
    }

    return false;
}


// wifi 状态机初始化
void Wifi::TopLayerSMInit()
{
    // 待机
    TopLayerSM.AddStateNode(WifiTopLayerState_off, [](StateMachine::StateType beforState) -> StateMachine::StateType {
        return WifiTopLayerState_off;
    });
    TopLayerSM.LinkState(WifiTopLayerState_off, WifiTopLayerState_off, []() {});

    // 错误
    TopLayerSM.AddStateNode(WifiTopLayerState_error, [](StateMachine::StateType beforState) -> StateMachine::StateType
    {
        static bool timeoutFlag = false;
        // 从其他节点跳转而来
        if (WifiTopLayerState_error != beforState) {
            logd("WifiTopLayerState_error from : %d", beforState);

            // 退出 透传模式
            TxMsgDef msg;
            msg.size = sprintf((char*)msg.data, "+++");
            self.TxQueue.push(msg);

            // 添加一个定时器
            timeoutFlag = false;
            self.timerComponent.AddTimer(555, []() {
                timeoutFlag = true;
            });
        }
        // 等待完成
        if (timeoutFlag == false) {
            return WifiTopLayerState_error;
        }
        // 前往复位环节
        if (self.Flags.isEN)
        {
            // 退出 透传模式
            TxMsgDef msg;
            msg.size = sprintf((char*)msg.data, "AT+CIPMODE=0\r\n");
            self.TxQueue.push(msg);
            return WifiTopLayerState_reset;
        }
        return WifiTopLayerState_error;
    });
    TopLayerSM.LinkState(WifiTopLayerState_error, WifiTopLayerState_error, []() {});
    TopLayerSM.LinkState(WifiTopLayerState_error, WifiTopLayerState_reset, []() {});

    // 复位
    TopLayerSM.AddStateNode(WifiTopLayerState_reset, [](StateMachine::StateType beforState) -> StateMachine::StateType
    {
        static bool timeoutFlag = false;
        // 从其他节点跳转而来
        if (beforState != WifiTopLayerState_reset) {

            logd("wifi reset begin...");
            // 强制切换到 AT 模式
            self.Flags.isAT_Mode = true;
            self.Flags.isTT_Mode = false;

            self.Respones.Clear();
            TxMsgDef msg;
            msg.size = sprintf((char*)msg.data, "AT+RST\r\n");
            self.TxQueue.push(msg);
            // 添加一个定时器
            timeoutFlag = false;
            self.timerComponent.AddTimer(1000, []() {
                timeoutFlag = true;
            });
        }
        if (timeoutFlag == false) {

            return WifiTopLayerState_reset;
        }
        return WifiTopLayerState_setup;
    });
    TopLayerSM.LinkState(WifiTopLayerState_reset, WifiTopLayerState_reset, []() {});
    TopLayerSM.LinkState(WifiTopLayerState_reset, WifiTopLayerState_setup, []() {});

    // 启动
    TopLayerSM.AddStateNode(WifiTopLayerState_setup, [](StateMachine::StateType beforState) -> StateMachine::StateType {
        static bool timeoutFlag = false;
        static int timerHandle = 0;
        // 从其他节点跳转而来
        if (beforState != WifiTopLayerState_setup) {
            logd("wifi setup begin...");
            // 清空响应 必须先于发送指令执行
            self.Respones.Clear();
            // 发送指令
            TxMsgDef msg;
            msg.size = sprintf((char*)msg.data, "AT\r\n");
            self.TxQueue.push(msg);
            // 添加一个定时器
            timeoutFlag = false;
            timerHandle = self.timerComponent.AddTimer(4000, []() {
                timeoutFlag = true;
            });
        }
        // 收到ok响应
        if (self.Respones.ok) {
            self.Flags.isRun = true;
            self.timerComponent.DelTimer(timerHandle);
            return WifiTopLayerState_AT_check;
        }
        else if (self.Respones.error) // 收到 error 响应
        {
            self.timerComponent.DelTimer(timerHandle);
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
    TopLayerSM.LinkState(WifiTopLayerState_setup, WifiTopLayerState_setup, []() {});
    TopLayerSM.LinkState(WifiTopLayerState_setup, WifiTopLayerState_error, []() {});
    TopLayerSM.LinkState(WifiTopLayerState_setup, WifiTopLayerState_AT_check, []() {});

    // 检查是否需要处理AT指令
    TopLayerSM.AddStateNode(WifiTopLayerState_AT_check, [](int befor)->int {
        static bool timeoutFlag = false;
        static int timerHandle = 0;
        if (self.ATFlags.notEmpty) {
            if (self.Flags.isAT_Mode) {
                // 当前已经是 AT 模式 直接进入AT操作
                return WifiTopLayerState_AT;
            }
            else {
                // 当前为 TT 模式 先退出 TT 模式
                // 退出之前至少等待 200ms
                if (befor != WifiTopLayerState_AT_check) {
                    timeoutFlag = false;
                    timerHandle = self.timerComponent.AddTimer(200, []() {
                        timeoutFlag = true;
                    });
                }
                if (timeoutFlag == false) {
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
    TopLayerSM.LinkState(WifiTopLayerState_AT_check, WifiTopLayerState_AT_check, []() {});
    TopLayerSM.LinkState(WifiTopLayerState_AT_check, WifiTopLayerState_AT, []() {});
    TopLayerSM.LinkState(WifiTopLayerState_AT_check, WifiTopLayerState_TT_exit, []() {});
    TopLayerSM.LinkState(WifiTopLayerState_AT_check, WifiTopLayerState_TT_check, []() {});

    // 退出透传模式
    TopLayerSM.AddStateNode(WifiTopLayerState_TT_exit, [](int befor)->int {
        static bool timeoutFlag = false;
        static int timerHandle = 0;
        // first entry
        if (befor != WifiTopLayerState_TT_exit) {
            logd("wifi TT exit ...");
            // 退出透传模式
            TxMsgDef msg;
            msg.size = sprintf((char*)msg.data, "+++");
            self.TxQueue.push(msg);
            // 等待
            timeoutFlag = false;
            timerHandle = self.timerComponent.AddTimer(300, []() {
                timeoutFlag = true;
            });
        }
        if (timeoutFlag == false)
        {
            self.Flags.isTT_Mode = false;
            return WifiTopLayerState_TT_exit;
        }
        return WifiTopLayerState_AT_entry;
    });
    TopLayerSM.LinkState(WifiTopLayerState_TT_exit, WifiTopLayerState_TT_exit, []() {});
    TopLayerSM.LinkState(WifiTopLayerState_TT_exit, WifiTopLayerState_AT_entry, []() {});


    // 进入AT模式
    TopLayerSM.AddStateNode(WifiTopLayerState_AT_entry, [](int befor)->int {
        static bool timeoutFlag = false;
        static int timerHandle = -1;
        // first entry
        if (befor != WifiTopLayerState_AT_entry) {
            logd("wifi AT entry ...");
            // 清空响应 必须先于发送指令执行
            self.Respones.Clear();
            // 退出 透传模式
            TxMsgDef msg;
            msg.size = sprintf((char*)msg.data, "AT+CIPMODE=0\r\n");
            self.TxQueue.push(msg);
            //
            timeoutFlag = false;
            timerHandle = self.timerComponent.AddTimer(4000, []() {
                timeoutFlag = true;
            });
        }
        // 响应 OK
        if (self.Respones.ok)
        {
            self.Flags.isAT_Mode = true;
            return WifiTopLayerState_AT;
        }
        else if (self.Respones.error) // 响应 error
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
    TopLayerSM.LinkState(WifiTopLayerState_AT_entry, WifiTopLayerState_AT_entry, []() {});
    TopLayerSM.LinkState(WifiTopLayerState_AT_entry, WifiTopLayerState_AT, []() {});
    TopLayerSM.LinkState(WifiTopLayerState_AT_entry, WifiTopLayerState_error, []() {});

    // 执行AT操作
    TopLayerSM.AddStateNode(WifiTopLayerState_AT, [](int befor)->int {
        static bool isRun = false;
        //
        if (befor != WifiTopLayerState_AT) {
            logd("wifi AT exec ...");
            // AT操作选择器
            if (self.WifiSmAtSelect()) {
                isRun = true;
                return WifiTopLayerState_AT;
            }

            // 没有找到指令 跳过 AT 执行过程
            isRun = false;
        }
        if (isRun) {
            AsyncATEvent::Event e = self.asyncATEvent.Polling();
            // 事件进行中
            if (e == AsyncATEvent::EventNone) {
                return WifiTopLayerState_AT;
            }
            isRun = false;
            // 完成
            if (e == AsyncATEvent::EventOK) {
                return WifiTopLayerState_TT_check;
            }
            // 错误 | 超时
            // if(e == AsyncATEvent::EventError || e == AsyncATEvent::EventErrorTimeout || e== AsyncATEvent::EventErrorWrite) {
            // }
            if (e == AsyncATEvent::EventErrorTimeout) {
                logd("wifi AT exec timeout");
            }
            return WifiTopLayerState_error;
        }
        return WifiTopLayerState_TT_check;
    });
    TopLayerSM.LinkState(WifiTopLayerState_AT, WifiTopLayerState_AT, []() {});
    TopLayerSM.LinkState(WifiTopLayerState_AT, WifiTopLayerState_TT_check, []() {});
    TopLayerSM.LinkState(WifiTopLayerState_AT, WifiTopLayerState_error, []() {});


    // 检查是否需要执行透传
    TopLayerSM.AddStateNode(WifiTopLayerState_TT_check, [](int befor)->int {
        if (self.Flags.isConnectAP && self.Flags.isSocketConnected) {
            // 当前处于AT模式 需要退出AT模式
            if (self.Flags.isAT_Mode) {
                return WifiTopLayerState_AT_exit;
            }
            // 当前为 TT 模式 直接进入透传
            return WifiTopLayerState_TT;
        }
        return WifiTopLayerState_AT_check;
    });
    TopLayerSM.LinkState(WifiTopLayerState_TT_check, WifiTopLayerState_TT, []() {});
    TopLayerSM.LinkState(WifiTopLayerState_TT_check, WifiTopLayerState_AT_check, []() {});
    TopLayerSM.LinkState(WifiTopLayerState_TT_check, WifiTopLayerState_AT_exit, []() {});

    // 退出 AT 模式
    TopLayerSM.AddStateNode(WifiTopLayerState_AT_exit, [](int befor)->int {
        if (befor != WifiTopLayerState_AT_exit) {
            logd("wifi AT exit ...");
            // 清空响应 必须先于发送指令执行
            self.Respones.Clear();
            // 进入(透传接收模式)
            self.asyncATEvent.Init("AT+CIPMODE=1\r\n", [](AsyncATEvent::Event event) {}, 4000, true);
        }
        AsyncATEvent::Event e = self.asyncATEvent.Polling();
        if (e == AsyncATEvent::EventNone) {
            return WifiTopLayerState_AT_exit;
        }
        if (e == AsyncATEvent::EventOK) {
            self.Flags.isAT_Mode = false;
            return wifiTopLayerState_TT_entry;
        }
        // 超时
        return WifiTopLayerState_error;
    });
    TopLayerSM.LinkState(WifiTopLayerState_AT_exit, WifiTopLayerState_AT_exit, []() {});
    TopLayerSM.LinkState(WifiTopLayerState_AT_exit, WifiTopLayerState_error, []() {});
    TopLayerSM.LinkState(WifiTopLayerState_AT_exit, wifiTopLayerState_TT_entry, []() {});


    // 切换透传
    TopLayerSM.AddStateNode(wifiTopLayerState_TT_entry, [](int befor)->int {
        if (befor != wifiTopLayerState_TT_entry) {
            logd("wifi entry TT Mode ...");
            // 清空响应 必须先于发送指令执行
            self.Respones.Clear();
            // 进入透传模式
            self.asyncATEvent.Init("AT+CIPSEND\r\n", [](AsyncATEvent::Event event) {}, 100, false);
        }
        // 等待响应
        AsyncATEvent::Event e = self.asyncATEvent.Polling();
        if (e == AsyncATEvent::EventNone) {
            return wifiTopLayerState_TT_entry;
        }
        // 响应 OK
        if (e == AsyncATEvent::EventOK) {
            self.Flags.isTT_Mode = true;
            return WifiTopLayerState_TT;
        }
        // 超时
        return WifiTopLayerState_error;
    });
    TopLayerSM.LinkState(wifiTopLayerState_TT_entry, wifiTopLayerState_TT_entry, []() {});
    TopLayerSM.LinkState(wifiTopLayerState_TT_entry, WifiTopLayerState_error, []() {});
    TopLayerSM.LinkState(wifiTopLayerState_TT_entry, WifiTopLayerState_TT, []() {});


    // 进行透传操作
    TopLayerSM.AddStateNode(WifiTopLayerState_TT, [](int befor)->int {
        logd("wifi TT exec ... %d %d", self.Flags.isAT_Mode, self.Flags.isTT_Mode);
        return  WifiTopLayerState_AT_check;
    });
    TopLayerSM.LinkState(WifiTopLayerState_TT, WifiTopLayerState_AT_check, []() {});

    // 默认为关闭状态
    TopLayerSM.Goto(WifiTopLayerState_off);
}

// 写
int Wifi::write(uint8_t* data, int len) {
    static OVERLAPPED wol = { 0 };
    static BOOL writeState;
    static DWORD writeSize;

    ResetEvent(wol.hEvent);
    WriteFile(ttys, data, len, &writeSize, &wol);
    return len;
}

// 读
int Wifi::read(uint8_t* buffer, int len) {
    static OVERLAPPED rol = { 0 };
    static DWORD readSize;

    ResetEvent(rol.hEvent);
    ReadFile(ttys, buffer, len, &readSize, &rol);
    return readSize;
}


// 循环处理
bool Wifi::Loop() {
    static uint8_t rxBuf[1024];
    static uint8_t rxBufTT[1024];
    static int rxBufRemainSize = 0;
    static int readSize;
    static bool rxBufRemainFlag;

    // 定时器组件处理
    timerComponent.TimerLoop();

    // 顶层状态机
    TopLayerSM.RunLoop();

    // 写串口
    // AT 模式
    if (Flags.isTT_Mode) { // 透传模式
        // 发送消息队列中的所有数据
        while (TxQueueTT.empty() == false)
        {
            TxMsgDef& msg = TxQueueTT.front();
            logd("TT_TX >> %s", msg.data);
            write(msg.data, msg.size);
            TxQueueTT.pop();
        }
    }
    else {
        // 发送消息队列中的所有数据
        while (TxQueue.empty() == false)
        {
            TxMsgDef& msg = TxQueue.front();
            logd("TX >> %s", msg.data);
            write(msg.data, msg.size);
            TxQueue.pop();
        }
    }

    // 读串口
    // AT 模式
    if (Flags.isAT_Mode) {
        readSize = read(rxBuf + rxBufRemainSize, 1024);
        //异步不需要处理返回值
        if (readSize < 1)
        {
            return true;
        }
        readSize += rxBufRemainSize;
        rxBuf[readSize] = 0;
        logd("AT_RX size: %d", readSize);

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
            logd("AT_RX line >> %s", line);

            // 遍历c++ map
            for (auto& it : Event.EventPool)
            {
                if (TextUtil::WithStart(line, it.first.c_str()))
                {
                    Event.Exec(it.first.c_str(), (uint8_t*)line, strlen(line));
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
    else { //透传接收模式
        readSize = read(rxBufTT, sizeof(rxBufTT) - 1);
        if (readSize < 1) {
            return true;
        }
        rxBufTT[readSize] = 0;
        logd("TT_RX >>> %s", rxBufTT);
    }

    return true;
}