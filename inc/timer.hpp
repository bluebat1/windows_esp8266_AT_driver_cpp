#pragma once
#include <time.h>
#include <vector>
#include <mutex>

using namespace std;

extern long long get_milliseconds();
// 定时器
class TimerComponent {
public:

    // 定时器回调
    typedef void(*Hook)();

    // 定时器结构体
    typedef struct {
        int handle;
        time_t endTime;
        Hook hook;
        bool isDelete;
    }Timer;

    // 池锁
    mutex poolMutex;

    // 定时器池
    vector<Timer> TimerPool;
    int timerCount = 0;

    TimerComponent() {
        TimerPool.clear();
    };
    ~TimerComponent() {
        TimerPool.clear();
    };

    // 添加一个定时器
    int AddTimer(int milliseconds, Hook _hook);

    // 删除一个定时器
    void DelTimer(int handle);

    // 定时器循环
    void TimerLoop();

};
