/**
 * @file	src\imcore.c
 *
 * @brief	imcore class.
 */
#include "imcore.h"
#include "im-thread.h"

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <event2/event.h>

#include "common.h"
#include "xmpp.h"
#include "mm.h"

static void _mem_leak_cb(void *p, size_t s, const char *file, uint64_t line, void *mem_userdata,
                         void *cb_userdata)
{
    printf("Memory leak size: %d, addr: %x. %s, %d.  \n", s, p, file, line);
    free(p);
}

bool im_init()
{
    safe_mem_init();
    im_thread_init();				// 已经包含可能的winsock库初始化
    xmpp_initialize();

    // 加载openssl库
    SSL_library_init();
    ERR_load_crypto_strings();
    SSL_load_error_strings();

    // 规定必须调用一次RAND_poll
    if (RAND_poll() == 0)
        assert(false);

    return true;
}

void im_destroy()
{
    xmpp_shutdown();
    im_thread_destroy();

    libevent_global_shutdown();                 // 释放libevent库资源, 2.1新加的方法
    safe_mem_check(_mem_leak_cb, NULL);         // 最后检查内存
}