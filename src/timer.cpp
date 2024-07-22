#include "timer.hpp"

using namespace std;

// 获取毫秒时间戳
long long get_milliseconds() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (ts.tv_sec * 1000LL) + (ts.tv_nsec / 1000000);
}

// 添加一个定时器
int TimerComponent::AddTimer(int milliseconds, TimerComponent::Hook _hook) {
    lock_guard<mutex> lock(poolMutex);
    timerCount++;
    Timer _t = {
        .handle = timerCount,
        .endTime = get_milliseconds() + milliseconds,
        .hook = _hook,
        .isDelete = false
    };
    TimerPool.push_back(_t);
    return timerCount;
}

// 删除一个定时器
void TimerComponent::DelTimer(int handle) {
    lock_guard<mutex> lock(poolMutex);
    for (int i = 0; i < TimerPool.size(); i++) {
        if (TimerPool[i].handle == handle) {
            // 标记为删除
            TimerPool[i].isDelete = true;
            // TimerPool.erase(TimerPool.begin() + i);
            break;
        }
    }
}

// 定时器循环
void TimerComponent::TimerLoop() {
    lock_guard<mutex> lock(poolMutex);
    int index = 0;
    for (int i = 0; i < TimerPool.size(); i++) {
        Timer& t = TimerPool[index];
        if (t.endTime < get_milliseconds()) {
            if (t.isDelete == false) {
                t.hook();
            }
            TimerPool.erase(TimerPool.begin() + index);
            index--;
        }
        index++;
    }
}
