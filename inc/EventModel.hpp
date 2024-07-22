#ifndef _EventModel_h_
#define _EventModel_h_

#include <map>
#include <string>
#include <stdint.h>

using namespace std;

class EventModel
{
private:
    /* data */
public:
    EventModel(/* args */){};
    ~EventModel(){};

    typedef void(*Hook)(uint8_t *data, int size);

    // 事件池
    map<string, Hook> EventPool;

    // 监听事件
    void ListenEvent(string event, Hook hook){
        EventPool[event] = hook;
    }
    // 移除事件
    void RemoveEvent(string event){
        EventPool.erase(event);
    }
    // 触发事件
    bool Exec(string event, uint8_t * data, int size){
        auto e = EventPool.find(event);
        if(e == EventPool.end()) {
            return false;
        }
        EventPool[event](data, size);
        return true;
    }

};

#endif // !_EventModel_h_
