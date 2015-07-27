#include "imcore.h"

#include <event2/event.h>
#include <event2/thread.h>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>

#include "mm.h"
#include "im-thread.h"
#include "xmpp.h"

struct im_conn {
    xmpp_ctx_t  *xmpp;
    im_thread_t *signal_thread;
    im_thread_t *work_thread;
    void *userdata;
};

im_conn_t *im_conn_new(const char *host,
                       const char *username,
                       const char *password,
                       im_conn_state_cb statecb,
                       im_conn_recive_cb msgcb,
                       void *userdate)
{
    assert(username && password && statecb && msgcb);
    if (!username || !password || !statecb || !msgcb)
        return NULL;

    im_conn_t *conn = safe_mem_malloc(sizeof(im_conn_t), NULL);
    if (!conn)
        return NULL;

    conn->signal_thread = im_thread_new();
    if (!conn->signal_thread) {
        safe_mem_free(conn);
        return NULL;
    }

    conn->work_thread = im_thread_new();
    if (!conn->work_thread) {
        im_thread_free(conn->signal_thread);
        safe_mem_free(conn);
        return NULL;
    }


}