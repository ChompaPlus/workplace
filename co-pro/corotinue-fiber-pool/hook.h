#ifndef _HOOK_H_
#define _HOOK_H_

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>          
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <fcntl.h>

namespace sylar{

bool is_hook_enable();
void set_hook_enable(bool flag);

}

extern "C"// c风格的代码
{
	// track the original version
	// typedef定义了一个返回类型为unsigned int、参数类型为 unsigned int的函数指针
	// 利用该指针声明了一个对外的函数变量名称，代表该变量就是一个返回类型和参数类型和上述指针相同的函数
	typedef unsigned int (*sleep_fun) (unsigned int seconds);
	extern sleep_fun sleep_f;

	typedef int (*usleep_fun) (useconds_t usec);
	extern usleep_fun usleep_f;

	    // nanosleep,精度是纳秒，可以指定线程的休眠时间，如果休眠被中断，会返回剩余休眠时间
	//// nanosleep,精度是纳秒，可以指定线程的休眠时间，如果休眠被中断，会返回剩余休眠时间
	typedef int (*nanosleep_fun) (const struct timespec* req, struct timespec* rem);
	extern nanosleep_fun nanosleep_f;	

	// pf_inet/pf_inet6
	// 成功返回一个socketfd， 失败返回-1及设置errno
	 // 创建socket（使用的底层协议tcp对应ipv4或6，服务类型tcp/udp（socketstream/UGRAM），0）
	typedef int (*socket_fun) (int domain, int type, int protocol);
	extern socket_fun socket_f;

	// 客户端主动发起连接，成功返回0 失败同上
	// // 客户端主动发起连接（socket系统调用返回socket，服务器监听的socket地址，地址长度）
	// 简单来说就是客户端要发起连接的socket， 他的地址，及地址长度
	typedef int (*connect_fun) (int sockfd, const struct sockaddr *addr, socklen_t addrlen);
	extern connect_fun connect_f;

	// 服务端主动接受连接成功返回一个新的连接socketfd
	// 执行liste调用的socket，addr是客户端被接受连接的socket地址，及被接受连接的地址长度
	typedef int (*accept_fun) (int sockfd, struct sockaddr *addr, socklen_t *addrlen);
	extern accept_fun accept_f;

	// 文件读
	typedef ssize_t (*read_fun) (int fd, void *buf, size_t count);
	extern read_fun read_f;

	typedef ssize_t (*readv_fun)(int fd, const struct iovec *iov, int iovcnt);
	extern readv_fun readv_f;

	// 通信数据读
	typedef ssize_t (*recv_fun) (int sockfd, void *buf, size_t len, int flags);
	extern recv_fun recv_f;

	typedef ssize_t (*recvfrom_fun) (int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);
	extern recvfrom_fun recvfrom_f;

	typedef ssize_t (*recvmsg_fun) (int sockfd, struct msghdr *msg, int flags);
	extern recvmsg_fun recvmsg_f;

	// 文件写
	typedef ssize_t (*write_fun) (int fd, const void *buf, size_t count);
	extern write_fun write_f;

	typedef ssize_t (*writev_fun) (int fd, const struct iovec *iov, int iovcnt);
	extern writev_fun writev_f;

	// 通信写
	typedef ssize_t (*send_fun) (int sockfd, const void *buf, size_t len, int flags);
	extern send_fun send_f;

	typedef ssize_t (*sendto_fun) (int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen);
	extern sendto_fun sendto_f;

	typedef ssize_t (*sendmsg_fun) (int sockfd, const struct msghdr *msg, int flags);
	extern sendmsg_fun sendmsg_f;

	// 关闭socket连接（传入要关闭的socketfd）
	typedef int (*close_fun) (int fd);
	extern close_fun close_f;

	// 
	typedef int (*fcntl_fun) (int fd, int cmd, ... /* arg */ );
	extern fcntl_fun fcntl_f;

	typedef int (*ioctl_fun) (int fd, unsigned long request, ...);
	extern ioctl_fun ioctl_f;

	typedef int (*getsockopt_fun) (int sockfd, int level, int optname, void *optval, socklen_t *optlen);
    extern getsockopt_fun getsockopt_f;

    typedef int (*setsockopt_fun) (int sockfd, int level, int optname, const void *optval, socklen_t optlen);
    extern setsockopt_fun setsockopt_f;

    // function prototype -> 对应.h中已经存在 可以省略
	// sleep function 
	unsigned int sleep(unsigned int seconds);
	int usleep(useconds_t usce);
	int nanosleep(const struct timespec* req, struct timespec* rem);

	// socket funciton
	int socket(int domain, int type, int protocol);
	int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
	int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);

// | 函数        | 第一个参数 `sockfd` 是什么     |
// | --------- | ---------------------- |
// | `connect` | 一个主动连接的 socket（通常是客户端） |
// | `accept`  | 一个监听 socket（通常是服务器）    |

	// read 
	ssize_t read(int fd, void *buf, size_t count);
	ssize_t readv(int fd, const struct iovec *iov, int iovcnt);

    ssize_t recv(int sockfd, void *buf, size_t len, int flags);
    ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);
    ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags);

    // write
    ssize_t write(int fd, const void *buf, size_t count);
    ssize_t writev(int fd, const struct iovec *iov, int iovcnt);

    ssize_t send(int sockfd, const void *buf, size_t len, int flags);
    ssize_t sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen);
    ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags);

    // fd
    int close(int fd);

    // socket control
    int fcntl(int fd, int cmd, ... /* arg */ );
    int ioctl(int fd, unsigned long request, ...);

    int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen);
    int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
}
#endif