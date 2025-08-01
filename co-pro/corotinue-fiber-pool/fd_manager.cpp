#include "fd_manager.h"
#include "hook.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

namespace sylar{

// instantiate
template class Singleton<FdManager>;

// Static variables need to be defined outside the class
template<typename T>
T* Singleton<T>::instance = nullptr;

template<typename T>
std::mutex Singleton<T>::mutex;	

FdCtx::FdCtx(int fd):
m_fd(fd)
{
	init();
}

FdCtx::~FdCtx()
{

}

bool FdCtx::init()
// 过程就是：先将文件描述符的状态存起来，然后判断是否为socket套接字
// 是sockte套接字就把他设置成系统非阻塞的
{
	if(m_isInit)
	{
		return true;
	}
	
	struct stat statbuf;
	// fd is in valid
	// fstat函数用于将m_fd关联的文件描述符状态存放到statbuf中（存进来也是需要用方法来查看是否为socket）
	// 返回-1代表fd无效或者错误
	if(-1==fstat(m_fd, &statbuf))
	{
		m_isInit = false;
		m_isSocket = false;
	}
	else
	{
		m_isInit = true;	
		m_isSocket = S_ISSOCK(statbuf.st_mode);	// 用于判断是否为套接字（socket在创建成功时返回的整数）
		// S_ISSOCK是一个宏专门查看stat.st_mode位，然后判断是否位套接字
	}

	// if it is a socket -> set to nonblock
	if(m_isSocket)// 如果是套接字就设置成系统非阻塞的
	{
		// fcntl_f() -> the original fcntl() -> get the socket info
		int flags = fcntl_f(m_fd, F_GETFL, 0);// 获取套接字的状态

		if(!(flags & O_NONBLOCK)) // 
		{
			// if not -> set to nonblock
			fcntl_f(m_fd, F_SETFL, flags | O_NONBLOCK); // 非阻塞
		}
		m_sysNonblock = true;
	}
	else
	{
		m_sysNonblock = false;
	}

	return m_isInit;
}

void FdCtx::setTimeout(int type, uint64_t v)
{
	if(type==SO_RCVTIMEO) // 读就设置读超时时间
	{
		m_recvTimeout = v;
	}
	else
	{
		m_sendTimeout = v;
	}
}

uint64_t FdCtx::getTimeout(int type)
{
	if(type==SO_RCVTIMEO)
	{
		return m_recvTimeout;
	}
	else
	{
		return m_sendTimeout;
	}
}

FdManager::FdManager()
{
	m_datas.resize(64);// fdmananger管理的fd对象的数量
}

std::shared_ptr<FdCtx> FdManager::get(int fd, bool auto_create)
{
	if(fd==-1)
	{
		return nullptr;
	}

	std::shared_lock<std::shared_mutex> read_lock(m_mutex);
	if(m_datas.size() <= fd)
	{
		if(auto_create==false)
		{
			return nullptr;
		}
	}
	else
	{
		if(m_datas[fd]||!auto_create)
		{
			return m_datas[fd];
		}
	}

	read_lock.unlock();
	std::unique_lock<std::shared_mutex> write_lock(m_mutex);

	if(m_datas.size() <= fd)
	{
		m_datas.resize(fd*1.5);
	}

	m_datas[fd] = std::make_shared<FdCtx>(fd);
	return m_datas[fd];

}

void FdManager::del(int fd)
{
	std::unique_lock<std::shared_mutex> write_lock(m_mutex);
	if(m_datas.size() <= fd)
	{
		return;
	}
	m_datas[fd].reset();
}

}