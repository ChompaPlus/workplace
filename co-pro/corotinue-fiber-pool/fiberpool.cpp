// fiber_pool.cpp
#include "fiberpool.h"
#include "scheduler.h"

namespace sylar
{

// 每个线程一个协程池    
static thread_local std::shared_ptr<FiberPool> t_threadLocalFiberPool = nullptr;

std::shared_ptr<FiberPool> FiberPool::getLocalFiberPool() {
    if (!t_threadLocalFiberPool) {
        // 每个线程创建独立 FiberPool 实例
        t_threadLocalFiberPool = std::make_shared<FiberPool>(256);
    }
    return t_threadLocalFiberPool;
}


FiberPool::FiberPool(size_t fibers) 
    : m_poolSize(fibers) {
    createFibers(fibers);
}

FiberPool::~FiberPool() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto fiber : m_allFibers) {
        fiber.reset();
    }
}

std::shared_ptr<Fiber> FiberPool::acquire(std::function<void()> cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_idleFibers.empty()) {
        // 动态扩容策略：当池为空时，扩容50%
        size_t new_size = m_allFibers.size() * 1.5;
        createFibers(new_size - m_allFibers.size());
    }
    
    std::shared_ptr<Fiber> fiber = m_idleFibers.front();
    m_idleFibers.pop();
    
    // 复用协程对象
    fiber->reset(cb); 
    return fiber;
}

void FiberPool::release(std::shared_ptr<Fiber> fiber) { // 将协程设置为空闲态
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 重置状态并放回池中
    fiber->reset(nullptr); // 清除回调函数
    m_idleFibers.push(fiber); // 每次从协程池头部取协程，从尾部放入
}

void FiberPool::resize(size_t new_size) { // 重新设置协程池的大小
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (new_size > m_allFibers.size()) {
        createFibers(new_size - m_allFibers.size());
    } else if (new_size < m_allFibers.size()) {
        // 移除多余协程
        size_t remove_count = m_allFibers.size() - new_size;
        while (remove_count-- > 0 && !m_idleFibers.empty()) {
            std::shared_ptr<Fiber> fiber = m_idleFibers.front();
            m_idleFibers.pop();
            
            auto it = std::find(m_allFibers.begin(), m_allFibers.end(), fiber);
            if (it != m_allFibers.end()) {
                std::shared_ptr<Fiber> cur = *it;
                cur.reset();
                m_allFibers.erase(it);
            }
        }
    }
    
    m_poolSize = new_size;
}

void FiberPool::createFibers(size_t count) { // 真正创建协程池的地方
    for (size_t i = 0; i < count; ++i) {
        // 一开始是绑定了一个空函数
        std::shared_ptr<Fiber> fiber =std::make_shared<Fiber>([] {}, 0, true);
        fiber->reset(nullptr); // 初始化为空闲状态
        m_idleFibers.push(fiber);
        m_allFibers.push_back(fiber);
    }
}

size_t FiberPool::idleCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_idleFibers.size();
}

size_t FiberPool::totalCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_allFibers.size();
}



} // namespace sylar