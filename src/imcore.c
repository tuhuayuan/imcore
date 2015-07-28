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
    im_thread_init();				// �Ѿ��������ܵ�winsock���ʼ��
    xmpp_initialize();

    // ����openssl��
    SSL_library_init();
    ERR_load_crypto_strings();
    SSL_load_error_strings();

    // �涨�������һ��RAND_poll
    if (RAND_poll() == 0)
        assert(false);

    return true;
}

void im_destroy()
{
    xmpp_shutdown();
    im_thread_destroy();

    libevent_global_shutdown();                 // �ͷ�libevent����Դ, 2.1�¼ӵķ���
    safe_mem_check(_mem_leak_cb, NULL);         // ������ڴ�
}