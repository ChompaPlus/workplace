#ifndef _FIBERPOOL_H_
#define _FIBERPOOL_H_
#include "fiber.h"
#include <vector>
#include <queue>

namespace sylar
{
    
class FiberPool {
public:
    explicit FiberPool(size_t fibers = 256); // 默认协程池的协程数量为128
    ~FiberPool();
    
    std::shared_ptr<Fiber> acquire(std::function<void()> cb);// 从池子里拿协程

    void release(std::shared_ptr<Fiber> fiber); // 将工作完的协程放进池子（队列）

    void resize(size_t new_size); // 重新设置协程池的大小

    static std::shared_ptr<FiberPool> getLocalFiberPool();
    
    size_t idleCount() const;

    size_t totalCount() const;

private:
    void createFibers(size_t count);

    size_t m_poolSize; // 协程池的大小

    std::queue<std::shared_ptr<Fiber>> m_idleFibers; // 协程池中空闲协程的数量

    std::vector<std::shared_ptr<Fiber>> m_allFibers; // 存所有的协程应该是协程的总数量，空闲的和不空闲的

    mutable std::mutex m_mutex;
};
}
#endif
