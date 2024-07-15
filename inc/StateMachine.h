#ifndef _StateMachine_H_
#define _StateMachine_H_ 1

#include <stdio.h>
#include <map>
#include "util.h"


// #define logd(fmt, ...) printf("%s:%d >>  " fmt "\r\n", __FILE__, __LINE__, ##__VA_ARGS__)
// #define logd(fmt, ...) LOGD("%s:%d >>  " fmt , __FILE__, __LINE__, ##__VA_ARGS__)

using namespace std;

class StateMachine
{
private:
    /* data */
public:

    typedef int StateType;
    typedef StateType (*StateNodeHook)(StateType beforState);
    typedef void (*StateLinkHook)();

    StateMachine(/* args */){};
    ~StateMachine(){};

    // 状态节点映射
    map<StateType, StateNodeHook> stateMap;
    // 状态 节点 链接
    map<StateType, map<StateType, StateLinkHook> > stateLinkMap;

    // 强制中断状态
    StateType InterruptState = -1;
    // 前一刻的状态
    StateType beforState = -1;
    // 当前状态
    StateType nowState = -1;
    // 下一节点状态
    StateType NextState = -1;

    // 跳转到指定状态
    void Goto(StateType state);
    // 状态机循环函数
    // 需要被一直循环调用
    bool RunLoop();
    // 添加节点
    bool AddStateNode(StateType state, StateNodeHook hook);
    // 添加连线
    bool LinkState(StateType stateA, StateType stateB, StateLinkHook linkHook);
    // 停止状态机 
    bool Stop();
};

#endif // !_StateMachine_H_
