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
    safe_mem_init();            // ��ʼ����ȫ�ڴ�
    sock_initialize();          // ��ʼ��socket��
    im_thread_init();           // ��ʼ���߳̿�
    xmpp_initialize();          // ��ʼ��xmpp��
    
    // ����openssl��
    SSL_library_init();
    ERR_load_crypto_strings();
    SSL_load_error_strings();
    // �涨�������һ��RAND_poll
    if (RAND_poll() == 0) {
        fprintf(stderr, "RAND_poll() failed.\n");
    }
    
    return true;
}

void im_destroy()
{
    xmpp_shutdown();                            // �ر�xmpp��
    im_thread_destroy();                        // �ر��߳̿�
    sock_shutdown();                            // �ر�socket��
    
    libevent_global_shutdown();                 // �ͷ�libevent����Դ
    safe_mem_check(_mem_leak_cb, NULL);         // ������ڴ�
}
