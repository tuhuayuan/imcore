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

#include "common.h"


void sock_initialize(void);
void sock_shutdown(void);
int sock_error(void);

#endif // __IMCORE_XMPP_SOCK_H__ 
