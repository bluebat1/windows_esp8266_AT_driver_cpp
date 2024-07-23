#include "test.hpp"
#include "Wifi.hpp"

// 流程测试
// 1、重置
// 2、等待启动完成
// 3、扫描AP
// 4、等待扫描完成
// 5、连接AP
// 6、等待IP分配
// 7、连接 tcp
// 8、向TCP透传数据
// 9、接收来自TCP的透传数据
// 10、断开TCP
void Test1(){
    static int step = 0;
    // 
    switch (step)
    {
    case 0:
        // 使能
        Wifi::Instance().Flags.isEN = true;
        // 复位
        // Wifi::Instance().ATFlags.flag.reset = 1;
        // 
        Wifi::Instance().Flags.isRun = false;
        Wifi::Instance().TopLayerSM.Goto(Wifi::WifiTopLayerState_reset);
        step = 1;
        logd("step 0");
        break;
    case 1:
        // 等待启动完成
        if(Wifi::Instance().Flags.isRun){
            logd("step 1");
            step = 2;
        }
        break;
    case 2:
        // 扫描AP
        Wifi::Instance().Flags.isScanApOver = false;
        Wifi::Instance().ATFlags.flag.scanAP = 1;
        step = 3;
        logd("step 2");
        break;
    case 3:
        // 等待扫描完成
        if(Wifi::Instance().Flags.isScanApOver) {
            step = 4;
            logd("step 3");

        }
        break;
    case 4:
        // 连接AP
        Wifi::Instance().ConectApInfo.ssid = "HUAWEI-E518DV_Wi-Fi5";
        Wifi::Instance().ConectApInfo.password = "yuanyice2024-1-2";
        Wifi::Instance().ATFlags.flag.connectAP = 1;
        step = 5;
        logd("step 4");
        break;
    case 5:
        // 等待AP连接成功
        if(Wifi::Instance().Flags.isGotIP) {
            step = 6;
            logd("step 6");
            Wifi::Instance().ATFlags.flag.getIPInfo = 1;
        }
        break;
    case 6:
        // 连接TCP
        Wifi::Instance().SocketInfo.type = "TCP";
        Wifi::Instance().SocketInfo.host = "192.168.3.166";
        Wifi::Instance().SocketInfo.port = "8080";
        Wifi::Instance().ATFlags.flag.connectSocket = 1;
        step = 7;
        logd("step 7");
        break;
    case 7:
        // 等待连接成功
        if(Wifi::Instance().Flags.isSocketConnected) {
            step = 8;
            logd("step 8");
        }
        break;
    case 8:
        // 向TCP发送数据
        Wifi::TxMsgDef msg;
        msg.size = sprintf((char *)msg.data, "esp msg ... \r\n");
        Wifi::Instance().TxQueueTT.push(msg);
        step = 9;
        logd("step 9");
        
        // 等待5S
        Wifi::Instance().timerComponent.AddTimer(5000, [](){
            step = 10;
            logd("step 10");
        });
        break;
    case 9:
        break;
    case 10:
        // 断开socket连接
        Wifi::Instance().ATFlags.flag.disconnectSocket = 1;
        step = 11;
        logd("step 11");
        Wifi::Instance().timerComponent.AddTimer(5000, [](){
            step = 12;
            logd("step 12");
        });
        break;
    case 11:
        // 等待2S
        break;
    case 12:
        // 扫描wifi
        Wifi::Instance().Flags.isScanApOver = false;
        Wifi::Instance().ATFlags.flag.scanAP = 1;
        step = 13;
        logd("step 13");

        break;
    case 13:
        if(Wifi::Instance().Flags.isScanApOver) {
            step = 14;
            logd("step 14");
        }
        break;;
    case 14:
        // 断开AP连接
        Wifi::Instance().ATFlags.flag.disconnectAP = 1;
        step = 15;
        logd("step 15");
        break;
    default:
        break;
    }
}
