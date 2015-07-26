#include "imcore.h"
#include "sock.h"
#include "mm.h"
#include "im-thread.h"
#include "xmpp.h"

struct im_session {
    xmpp_ctx_t  *xmpp;
};

static void _mem_leak_cb(void *p, size_t s, const char *file, uint64_t line, void *mem_userdata,
                         void *cb_userdata)
{
    printf("Memory leak at size %d, addr %x. %s, %d.  \n", s, p, file, line);
    free(p);
}

bool im_init()
{
    safe_mem_init();            // 初始化安全内存
    sock_initialize();          // 初始化socket库
    im_thread_init();           // 初始化线程库
    xmpp_initialize();          // 初始化xmpp库
    
    // 加载openssl库
    SSL_library_init();
    ERR_load_crypto_strings();
    SSL_load_error_strings();
    // 规定必须调用一次RAND_poll
    if (RAND_poll() == 0) {
        fprintf(stderr, "RAND_poll() failed.\n");
    }
    
    return true;
}

void im_destroy()
{
    xmpp_shutdown();                            // 关闭xmpp库
    im_thread_destroy();                        // 关闭线程库
    sock_shutdown();                            // 关闭socket库
    
    libevent_global_shutdown();                 // 释放libevent库资源
    safe_mem_check(_mem_leak_cb, NULL);         // 最后检查内存
}
