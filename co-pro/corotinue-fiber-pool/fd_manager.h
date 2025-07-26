#ifndef _FD_MANAGER_H_
#define _FD_MANAGER_H_

#include <memory>
#include <shared_mutex>
#include "thread.h"


namespace sylar{

// fd info
class FdCtx : public std::enable_shared_from_this<FdCtx> // 用来管理fd的属性
{
private:
	bool m_isInit = false; // fd是否初始化
	bool m_isSocket = false; // fd是否时socket套接字
	bool m_sysNonblock = false; // fd是否设置为系统非阻塞
	bool m_userNonblock = false;// fd是否设置为用户非阻塞
	bool m_isClosed = false;// fd是否被关闭
	int m_fd;// fd的整数值，标识符

	// read event timeout
	uint64_t m_recvTimeout = (uint64_t)-1;//-1默认没有超时时间
	// write event timeout
	uint64_t m_sendTimeout = (uint64_t)-1;

public:
	FdCtx(int fd);
	~FdCtx();

	// 获取fd的状态
	bool init();
	bool isInit() const {return m_isInit;}
	bool isSocket() const {return m_isSocket;}
	bool isClosed() const {return m_isClosed;}
	// 设置fd
	void setUserNonblock(bool v) {m_userNonblock = v;}
	bool getUserNonblock() const {return m_userNonblock;}

	void setSysNonblock(bool v) {m_sysNonblock = v;}
	bool getSysNonblock() const {return m_sysNonblock;}
	// 设置和获取超时时间，type表示读或者写时间
	void setTimeout(int type, uint64_t v);
	uint64_t getTimeout(int type);
};

class FdManager
{
public:
	FdManager();
	// 获取指定的fd对象，不存在时自动创建auto——为true
	// 就是判断fd对象是否在数组里
	// 是否需要创建
	// 不在数组里就创建一个
	// 查看的时候是读锁，创建就写锁
	std::shared_ptr<FdCtx> get(int fd, bool auto_create = false);
	void del(int fd);

private:
	std::shared_mutex m_mutex;
	std::vector<std::shared_ptr<FdCtx>> m_datas;
};


template<typename T>// T将来用于单例的类
class Singleton
{
private:
    static T* instance;
    static std::mutex mutex;

protected:
    Singleton() {}  

public:
    // Delete copy constructor and assignment operation
	// 禁止拷贝和赋值操作，确保只有一个实例
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;

    static T* GetInstance() // 第一次调用时创建对象
    {
        std::lock_guard<std::mutex> lock(mutex); // Ensure thread safety
        if (instance == nullptr) 
        {
            instance = new T();
        }
        return instance;
    }

    static void DestroyInstance() 
    {
        std::lock_guard<std::mutex> lock(mutex);
        delete instance;
        instance = nullptr;
    }
};

typedef Singleton<FdManager> FdMgr;

}

#endif