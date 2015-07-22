/* sock.h
 * socket移植定义以及初始化部分，读写部分交给libevent
 */
#ifndef __IMCORE_XMPP_SOCK_H__
#define __IMCORE_XMPP_SOCK_H__

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#ifndef _WIN32
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#else
#include <winsock2.h>
#include <errno.h>
#endif

#if defined(_WIN32) && !defined(__cplusplus) && !defined(inline)
#define inline __inline
#endif

static inline void sock_initialize(void)
{
#ifdef _WIN32
    WSADATA wsad;
    WSAStartup(0x0101, &wsad);
#endif
}

static inline sock_shutdown(void)
{
#ifdef _WIN32
    WSACleanup();
#endif
}

static inline int sock_error(void)
{
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

// SRV记录查询
int sock_srv_lookup(const char *service, const char *proto, const char *domain,
                    char *resulttarget, int resulttargetlength, int *resultport);

#endif // __IMCORE_XMPP_SOCK_H__ 
