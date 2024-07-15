#include "StateMachine.h"

using namespace std;

// 跳转到指定状态
void StateMachine::Goto(StateType state){
	InterruptState = state;
}

// 状态机循环函数
// 需要被一直循环调用
bool StateMachine::RunLoop(){
	// 如果存在中断、强制将当前状态设为中断状态
	if(InterruptState >= 0){
		nowState = InterruptState;
		InterruptState = -1;
	}else{
		// 处理停止请求
		if(InterruptState == -2) {
			InterruptState = -1;
			nowState = -1;
		}
	}
	// 如何当前状态为默认 则不执行任何内容
	if (nowState < 0)
	{
		return true;
	}
	// 执行状态节点内容
	if(stateMap.find(nowState) == stateMap.end()) {
		logd("StateMachine Node error");
		return false;
	}
	NextState = stateMap[nowState](beforState);
	// 查找连接
	// 查找连接首端
	if(stateLinkMap.find(nowState) == stateLinkMap.end()) {
		logd("StateMachine Link port-A error . now state: %d ", nowState);
		return false;
	}
	// 查找连接末端
	map<StateType, StateLinkHook> & subLink = stateLinkMap[nowState];
	if(subLink.find(NextState) == subLink.end()) {
		logd("StateMachine Link port-B error . next state: %d", NextState);
		return false;
	}
	// 执行节点跳转钩子
	subLink[NextState]();
	// 记录前一个节点
	beforState = nowState;
	nowState = NextState;
	return true;
}

// 添加节点
bool StateMachine::AddStateNode(StateType state, StateNodeHook hook) {
	stateMap[state] = hook;
	return true;
}

// 添加连线
bool StateMachine::LinkState(StateType stateA, StateType stateB, StateLinkHook linkHook){
	// 如果 连线首端 不存在 则 创建新的首端
	// if(stateLinkMap.find(stateA) == stateLinkMap.end()) {
	//     logd("create link");
	//     stateLinkMap[stateA] = map<StateType, StateLinkHook>{};
	// }
	// 添加连接线并设置钩子
	stateLinkMap[stateA][stateB] = linkHook;
	return true;
}

// 停止状态机
bool StateMachine::Stop(){
	InterruptState = -2;
	return true;
}

