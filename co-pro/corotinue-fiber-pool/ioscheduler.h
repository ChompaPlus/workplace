#ifndef __SYLAR_IOMANAGER_H__
#define __SYLAR_IOMANAGER_H__

#include "scheduler.h"
#include "timer.h"

namespace sylar {

// work flow
// 1 register one event -> 2 wait for it to ready -> 3 schedule the callback -> 4 unregister the event -> 5 run the callback
class IOManager : public Scheduler, public TimerManager 
{
public:
    enum Event // 事件类型， none表示无事件或者读或者写或者读写的组合
    {
        NONE = 0x0,
        // READ == EPOLLIN
        READ = 0x1,
        // WRITE == EPOLLOUT
        WRITE = 0x4
    };

private:
    struct FdContext  // 这个是给每个fd初始化的信息和其上的事件具体是如何处理的
    // 1. 信息包括，fd的值，fd上面包含的事件有哪些，每个事件具体的执行上下文
    // 2. 主要执行过程就是，首先获取事件的执行上下文，然后触发事件（加入任务队列由工作线程获取交给协程执行），最后重置事件
    {
        struct EventContext // 每个事件具体的执行上下文， 关联的io调度器，需要执行的协程或者函数
        {
            // scheduler
            Scheduler *scheduler = nullptr;
            // callback fiber
            std::shared_ptr<Fiber> fiber;
            // callback function
            std::function<void()> cb;
        };

        // 只包含读写两种类型，意味着如果发生其他的边缘事件，都需要映射到读写的处理逻辑来执行任务
        // read event context
        EventContext read; 
        // write event context
        EventContext write;
        int fd = 0;
        // events registered
        Event events = NONE; // 这个就是存fd上的事件具体有哪些类型，以及触发方式
        std::mutex mutex;

        EventContext& getEventContext(Event event);
        void resetEventContext(EventContext &ctx);
        void triggerEvent(Event event);        
    };

public:
    IOManager(size_t threads = 1, bool use_caller = true, const std::string &name = "IOManager");
    ~IOManager();

    // add one event at a time
    // 向epoll事件表里注册fd上的某个事件，并绑定相应的回调函数（更新执行上下文）
    int addEvent(int fd, Event event, std::function<void()> cb = nullptr);
    // delete event
    bool delEvent(int fd, Event event);// 删除事件表里fd上的某个事件
    // delete the event and trigger its callback
    bool cancelEvent(int fd, Event event);// 取消fd上的某个事件并且触发他
    // delete all events and trigger its callback
    bool cancelAll(int fd);

    static IOManager* GetThis();

protected:
    void tickle() override;
    
    bool stopping() override;
    
    void idle() override;

    void onTimerInsertedAtFront() override;

    void contextResize(size_t size);

private:
    int m_epfd = 0;
    // fd[0] read，fd[1] write
    int m_tickleFds[2];
    std::atomic<size_t> m_pendingEventCount = {0};
    std::shared_mutex m_mutex;
    // store fdcontexts for each fd
    std::vector<FdContext *> m_fdContexts;
};

} // end namespace sylar

#endif