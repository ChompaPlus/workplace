#include "hook.h"
#include "ioscheduler.h"
#include <dlfcn.h>
#include <iostream>
#include <cstdarg>
#include "fd_manager.h"
#include <string.h>

// apply XX to all functions
// 就是成批的处理以下函数，XX就代表我们在头文件中定义的函数
// 对于sleep来说，XX就代表sleep_fun
// 利用宏定义就可以成批的将xx（我们设置的函数名称）对应到相应的库函数，并生成一系列代码
#define HOOK_FUN(XX) \
    XX(sleep) \
    XX(usleep) \
    XX(nanosleep) \
    XX(socket) \
    XX(connect) \
    XX(accept) \
    XX(read) \
    XX(readv) \
    XX(recv) \
    XX(recvfrom) \
    XX(recvmsg) \
    XX(write) \
    XX(writev) \
    XX(send) \
    XX(sendto) \
    XX(sendmsg) \
    XX(close) \
    XX(fcntl) \
    XX(ioctl) \
    XX(getsockopt) \
    XX(setsockopt) 

namespace sylar{

// if this thread is using hooked function 
static thread_local bool t_hook_enable = false; // 该线程是否已经hook住了

bool is_hook_enable() // 查看钩子的状态
{
    return t_hook_enable;
}

void set_hook_enable(bool flag)// 设置狗子的状态
{
    t_hook_enable = flag;
}

void hook_init() // 初始化钩子同时将函数展开，现在是有钩子但不确定是否使用钩子
                // 只要有钩子，我们定义的函数名称就能实现原库函数的功能
{
	static bool is_inited = false;
	if(is_inited)
	{
		return;
	}

	// test
	is_inited = true;

// 这里是真正将我们定义的函数名称关联到库函数，使他们可以实现相应的功能
// 动态库加载函数dlsym就是为了从共享库（动态库中）获取函数符号地址
// dlsym(void* handle, const char* symbol)
// 第一个参数可以是dlopen返回的值，也可以是RTLD_DEFAULT和RTLD_NEXT
// default就会从当前库开始找，可能会找到自己
// 区别就是next从当前库以后的库中开始寻找symbol符号第一次出现的地址，不会找到自己身上
// assignment -> sleep_f = (sleep_fun)dlsym(RTLD_NEXT, "sleep"); -> dlsym -> fetch the original symbols/function
#define XX(name) name ## _f = (name ## _fun)dlsym(RTLD_NEXT, #name);
	HOOK_FUN(XX)
#undef XX
}

// static variable initialisation will run before the main function
struct HookIniter
{
	HookIniter()
	{
		hook_init();
	}
};

static HookIniter s_hook_initer; //静态全局变量，在执行main之前就hook初始化

} // end namespace sylar

struct timer_info 
{
    int cancelled = 0;
};

// universal template for read and write function
template<typename OriginFun, typename... Args>
// 就是对系统调用进行封装实现非阻塞，如果出现资源暂时不可用的情况，就将该事件陷入epoll（到epoll上注册相应的事件，然后去监听），使用超时或者资源读或者事件写事件使用tickle去唤醒epoll，将任务放到调度器中去执行
static ssize_t do_io(int fd, OriginFun fun, const char* hook_fun_name, uint32_t event, int timeout_so, Args&&... args) 
{
    if(!sylar::t_hook_enable) 
    {
        return fun(fd, std::forward<Args>(args)...);
    }

    std::shared_ptr<sylar::FdCtx> ctx = sylar::FdMgr::GetInstance()->get(fd);
    if(!ctx) 
    {
        return fun(fd, std::forward<Args>(args)...);
    }

    if(ctx->isClosed()) 
    {
        errno = EBADF;
        return -1;
    }

    if(!ctx->isSocket() || ctx->getUserNonblock()) 
    {
        return fun(fd, std::forward<Args>(args)...);
    }

    // get the timeout
    uint64_t timeout = ctx->getTimeout(timeout_so);
    // timer condition
    // 记录定时器的状态
    std::shared_ptr<timer_info> tinfo(new timer_info);

retry:
	// run the function
    ssize_t n = fun(fd, std::forward<Args>(args)...);
    
    // EINTR ->Operation interrupted by system ->retry
    while(n == -1 && errno == EINTR) // 信号中断，尝试重新调用
    {
        n = fun(fd, std::forward<Args>(args)...);
    }
    
    // 0 resource was temporarily unavailable -> retry until ready 
    if(n == -1 && errno == EAGAIN) // 说明资源暂时不可用，陷入epoll
    {
        sylar::IOManager* iom = sylar::IOManager::GetThis();
        // timer
        std::shared_ptr<sylar::Timer> timer;
        std::weak_ptr<timer_info> winfo(tinfo);//用于记录定时器对象是否存活

        // 1 timeout has been set -> add a conditional timer for canceling this operation
        if(timeout != (uint64_t)-1) // 
        {
            timer = iom->addConditionTimer(timeout, [winfo, fd, iom, event]() 
            {
                auto t = winfo.lock();//用来将弱引用对象提升为一个shared_ptr成功则不为空
                if(!t || t->cancelled) 
                {
                    return;
                }
                t->cancelled = ETIMEDOUT;
                // cancel this event and trigger once to return to this fiber
                // 主动取消事件并触发任务
                iom->cancelEvent(fd, (sylar::IOManager::Event)(event));
            }, winfo);
        }

        // 2 add event -> callback is this fiber
        int rt = iom->addEvent(fd, (sylar::IOManager::Event)(event));
        if(rt) // 向m_epfd上注册该事件，成功rt=0
        {
            std::cout << hook_fun_name << " addEvent("<< fd << ", " << event << ")";
            if(timer) 
            {
                timer->cancel();
            }
            return -1;
        } 
        else 
        {
            sylar::Fiber::GetThis()->suspend();// 进入调度协程，由调度协程进入空闲协程，空闲协程监听
     
            // 3 resume either by addEvent or cancelEvent
            if(timer) // 表示还没有超过操作的超时时间，下面那个判断就不执行了
            {
                timer->cancel();
            }
            // by cancelEvent
            // 表示I/O操作失败
            if(tinfo->cancelled == ETIMEDOUT) 
            {
                errno = tinfo->cancelled;
                return -1; //
            }
            // 你打电话给快递小哥（系统调用），但他说他还在路上（EAGAIN），你挂了电话（yield），设置个闹钟 30 分钟后提醒你（定时器）。

            // 30 分钟内：

            // 快递到了，按门铃（epoll 触发） → 正常处理

            // 闹钟响了（定时器超时） → 你以为快递没来，记录为失败（ETIMEDOUT）

            // 你醒来（恢复协程）时要判断是门铃响还是闹钟响，然后分别处理——这段代码就是这个判断逻辑。
            goto retry; // 被唤醒后在执行一次系统调用， 如果资源已经准备好了就返回系统调用就不会在retry
        }
    }
    return n; // 
}



extern "C"{// 这里就是重定义我们函数功能的地方，此时钩子已经存在
            // 如果钩子的状态是不可用 is_hook_enable = false ，函数就使用原库函数的功能
            // 如果是true 就按照我们定义的方式使用函数

// declaration -> sleep_fun sleep_f = nullptr;
#define XX(name) name ## _fun name ## _f = nullptr;
	HOOK_FUN(XX)
#undef XX

// only use at task fiber
// 这些函数只用在工作协程也就是子协程上
unsigned int sleep(unsigned int seconds)
{
	if(!sylar::t_hook_enable)
	{
		return sleep_f(seconds);
	}

	std::shared_ptr<sylar::Fiber> fiber = sylar::Fiber::GetThis();
	sylar::IOManager* iom = sylar::IOManager::GetThis();
	// add a timer to reschedule this fiber
    // 用到了一个lamba匿名函数[获取的变量](){执行体}
    // 匿名函数本质上就是一个函数对象，可以自动转化成function<void()>
    // 就是将当前的执行任务封装成一个定时任务加入定时器
    // 具体就是当前协程需要等待sleep（）时间才能继续工作，，在这段时间将执行权让出来让其他协程工作
	iom->addTimer(seconds*1000, [fiber, iom](){iom->scheduleLock(fiber, -1);});
	// wait for the next resume
	fiber->yield();
	return 0;
}

int usleep(useconds_t usec)
{
	if(!sylar::t_hook_enable)
	{
		return usleep_f(usec);
	}

	std::shared_ptr<sylar::Fiber> fiber = sylar::Fiber::GetThis();
	sylar::IOManager* iom = sylar::IOManager::GetThis();
	// add a timer to reschedule this fiber
	iom->addTimer(usec/1000, [fiber, iom](){iom->scheduleLock(fiber);});
	// wait for the next resume
	fiber->yield();
	return 0;
}

int nanosleep(const struct timespec* req, struct timespec* rem)
{
	if(!sylar::t_hook_enable)
	{
		return nanosleep_f(req, rem);
	}	

	int timeout_ms = req->tv_sec*1000 + req->tv_nsec/1000/1000;

	std::shared_ptr<sylar::Fiber> fiber = sylar::Fiber::GetThis();
	sylar::IOManager* iom = sylar::IOManager::GetThis();
	// add a timer to reschedule this fiber
	iom->addTimer(timeout_ms, [fiber, iom](){iom->scheduleLock(fiber, -1);});
	// wait for the next resume
	fiber->yield();	
	return 0;
}

int socket(int domain, int type, int protocol)
{
	if(!sylar::t_hook_enable)
	{
		return socket_f(domain, type, protocol);
	}	

	int fd = socket_f(domain, type, protocol);
	if(fd==-1)
	{
		std::cerr << "socket() failed:" << strerror(errno) << std::endl;
		return fd;
	}
	sylar::FdMgr::GetInstance()->get(fd, true);
	return fd;
}

int connect_with_timeout(int fd, const struct sockaddr* addr, socklen_t addrlen, uint64_t timeout_ms) 
{
    if(!sylar::t_hook_enable) 
    {
        return connect_f(fd, addr, addrlen);
    }

    std::shared_ptr<sylar::FdCtx> ctx = sylar::FdMgr::GetInstance()->get(fd);
    if(!ctx || ctx->isClosed()) 
    {
        errno = EBADF;
        return -1;
    }

    if(!ctx->isSocket()) 
    {
        return connect_f(fd, addr, addrlen);
    }

    if(ctx->getUserNonblock()) 
    {

        return connect_f(fd, addr, addrlen);
    }

    // attempt to connect
    // 如果是socket还是系统非阻塞的
    int n = connect_f(fd, addr, addrlen);
    if(n == 0) 
    {
        return 0;
    } 
    else if(n != -1 || errno != EINPROGRESS) 
    {
        return n;
    }

    // wait for write event is ready -> connect succeeds
    sylar::IOManager* iom = sylar::IOManager::GetThis();
    std::shared_ptr<sylar::Timer> timer;
    std::shared_ptr<timer_info> tinfo(new timer_info);
    std::weak_ptr<timer_info> winfo(tinfo);

    if(timeout_ms != (uint64_t)-1) 
    {
        timer = iom->addConditionTimer(timeout_ms, [winfo, fd, iom]() 
        {
            auto t = winfo.lock();
            if(!t || t->cancelled) 
            {
                return;
            }
            t->cancelled = ETIMEDOUT;
            iom->cancelEvent(fd, sylar::IOManager::WRITE);
        }, winfo);
    }

    int rt = iom->addEvent(fd, sylar::IOManager::WRITE);
    if(rt == 0) 
    {
        sylar::Fiber::GetThis()->yield();

        // resume either by addEvent or cancelEvent
        if(timer) 
        {
            timer->cancel();
        }

        if(tinfo->cancelled) 
        {
            errno = tinfo->cancelled;
            return -1;
        }
    } 
    else 
    {
        if(timer) 
        {
            timer->cancel();
        }
        std::cerr << "connect addEvent(" << fd << ", WRITE) error";
    }

    // check out if the connection socket established 
    int error = 0;
    socklen_t len = sizeof(int);
    if(-1 == getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len)) 
    {
        return -1;
    }
    if(!error) 
    {
        return 0;
    } 
    else 
    {
        errno = error;
        return -1;
    }
}


static uint64_t s_connect_timeout = -1;
int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
	return connect_with_timeout(sockfd, addr, addrlen, s_connect_timeout);
}

int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
	int fd = do_io(sockfd, accept_f, "accept", sylar::IOManager::READ, SO_RCVTIMEO, addr, addrlen);	
	if(fd>=0)
	{
		sylar::FdMgr::GetInstance()->get(fd, true);
	}
	return fd;
}

ssize_t read(int fd, void *buf, size_t count)
{
	return do_io(fd, read_f, "read", sylar::IOManager::READ, SO_RCVTIMEO, buf, count);	
}

ssize_t readv(int fd, const struct iovec *iov, int iovcnt)
{
	return do_io(fd, readv_f, "readv", sylar::IOManager::READ, SO_RCVTIMEO, iov, iovcnt);	
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags)
{
	return do_io(sockfd, recv_f, "recv", sylar::IOManager::READ, SO_RCVTIMEO, buf, len, flags);	
}

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen)
{
	return do_io(sockfd, recvfrom_f, "recvfrom", sylar::IOManager::READ, SO_RCVTIMEO, buf, len, flags, src_addr, addrlen);	
}

ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags)
{
	return do_io(sockfd, recvmsg_f, "recvmsg", sylar::IOManager::READ, SO_RCVTIMEO, msg, flags);	
}

ssize_t write(int fd, const void *buf, size_t count)
{
	return do_io(fd, write_f, "write", sylar::IOManager::WRITE, SO_SNDTIMEO, buf, count);	
}

ssize_t writev(int fd, const struct iovec *iov, int iovcnt)
{
	return do_io(fd, writev_f, "writev", sylar::IOManager::WRITE, SO_SNDTIMEO, iov, iovcnt);	
}

ssize_t send(int sockfd, const void *buf, size_t len, int flags)
{
	return do_io(sockfd, send_f, "send", sylar::IOManager::WRITE, SO_SNDTIMEO, buf, len, flags);	
}

ssize_t sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen)
{
	return do_io(sockfd, sendto_f, "sendto", sylar::IOManager::WRITE, SO_SNDTIMEO, buf, len, flags, dest_addr, addrlen);	
}

ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags)
{
	return do_io(sockfd, sendmsg_f, "sendmsg", sylar::IOManager::WRITE, SO_SNDTIMEO, msg, flags);	
}

int close(int fd)
{
	if(!sylar::t_hook_enable)
	{
		return close_f(fd);
	}	

	std::shared_ptr<sylar::FdCtx> ctx = sylar::FdMgr::GetInstance()->get(fd);

	if(ctx)
	{
		auto iom = sylar::IOManager::GetThis();
		if(iom)
		{	
			iom->cancelAll(fd);
		}
		// del fdctx
		sylar::FdMgr::GetInstance()->del(fd);
	}
	return close_f(fd);
}

int fcntl(int fd, int cmd, ... /* arg */ )
{
  	va_list va; // to access a list of mutable parameters

    va_start(va, cmd);
    switch(cmd) 
    {
        case F_SETFL:
            {
                int arg = va_arg(va, int); // Access the next int argument
                va_end(va);
                std::shared_ptr<sylar::FdCtx> ctx = sylar::FdMgr::GetInstance()->get(fd);
                if(!ctx || ctx->isClosed() || !ctx->isSocket()) 
                {
                    return fcntl_f(fd, cmd, arg);
                }
                // 用户是否设定了非阻塞
                ctx->setUserNonblock(arg & O_NONBLOCK);
                // 最后是否阻塞根据系统设置决定
                if(ctx->getSysNonblock()) 
                {
                    arg |= O_NONBLOCK;
                } 
                else 
                {
                    arg &= ~O_NONBLOCK;
                }
                return fcntl_f(fd, cmd, arg);
            }
            break;

        case F_GETFL:
            {
                va_end(va);
                int arg = fcntl_f(fd, cmd);
                std::shared_ptr<sylar::FdCtx> ctx = sylar::FdMgr::GetInstance()->get(fd);
                if(!ctx || ctx->isClosed() || !ctx->isSocket()) 
                {
                    return arg;
                }
                // 这里是呈现给用户 显示的为用户设定的值
                // 但是底层还是根据系统设置决定的
                if(ctx->getUserNonblock()) 
                {
                    return arg | O_NONBLOCK;
                } else 
                {
                    return arg & ~O_NONBLOCK;
                }
            }
            break;

        case F_DUPFD:
        case F_DUPFD_CLOEXEC:
        case F_SETFD:
        case F_SETOWN:
        case F_SETSIG:
        case F_SETLEASE:
        case F_NOTIFY:
#ifdef F_SETPIPE_SZ
        case F_SETPIPE_SZ:
#endif
            {
                int arg = va_arg(va, int);
                va_end(va);
                return fcntl_f(fd, cmd, arg); 
            }
            break;


        case F_GETFD:
        case F_GETOWN:
        case F_GETSIG:
        case F_GETLEASE:
#ifdef F_GETPIPE_SZ
        case F_GETPIPE_SZ:
#endif
            {
                va_end(va);
                return fcntl_f(fd, cmd);
            }
            break;

        case F_SETLK:
        case F_SETLKW:
        case F_GETLK:
            {
                struct flock* arg = va_arg(va, struct flock*);
                va_end(va);
                return fcntl_f(fd, cmd, arg);
            }
            break;

        case F_GETOWN_EX:
        case F_SETOWN_EX:
            {
                struct f_owner_exlock* arg = va_arg(va, struct f_owner_exlock*);
                va_end(va);
                return fcntl_f(fd, cmd, arg);
            }
            break;

        default:
            va_end(va);
            return fcntl_f(fd, cmd);
    }	
}

int ioctl(int fd, unsigned long request, ...)
{
    va_list va;
    va_start(va, request);
    void* arg = va_arg(va, void*);
    va_end(va);

    if(FIONBIO == request) 
    {
        bool user_nonblock = !!*(int*)arg;
        std::shared_ptr<sylar::FdCtx> ctx = sylar::FdMgr::GetInstance()->get(fd);
        if(!ctx || ctx->isClosed() || !ctx->isSocket()) 
        {
            return ioctl_f(fd, request, arg);
        }
        ctx->setUserNonblock(user_nonblock);
    }
    return ioctl_f(fd, request, arg);
}

int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen)
{
	return getsockopt_f(sockfd, level, optname, optval, optlen);
}
// 设置socketfd的属性的方法，本项目拦截以后就是要设置给fd设置超时时间
int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen)
{
    if(!sylar::t_hook_enable) 
    {
        return setsockopt_f(sockfd, level, optname, optval, optlen);
    }

    if(level == SOL_SOCKET) // level代表指定要操作的协议选项
    {
        if(optname == SO_RCVTIMEO || optname == SO_SNDTIMEO) 
        {
            std::shared_ptr<sylar::FdCtx> ctx = sylar::FdMgr::GetInstance()->get(sockfd);
            if(ctx) 
            {
                const timeval* v = (const timeval*)optval;
                ctx->setTimeout(optname, v->tv_sec * 1000 + v->tv_usec / 1000);
            }
        }
    }
    return setsockopt_f(sockfd, level, optname, optval, optlen);	
}

}
