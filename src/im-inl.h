#ifndef __IMCORE_INL_H__
#define __IMCORE_INL_H__

#include "imcore.h"

#include <event2/event.h>
#include <event2/thread.h>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>

#include "mm.h"
#include "im-thread.h"
#include "xmpp.h"
#include "stringutils.h"

#include "im-msg.h"
#include "im-msg-text.h"
#include "im-msg-file.h"

struct im_conn {
    xmpp_ctx_t  *xmpp_ctx;
    xmpp_conn_t *xmpp_conn;
    char *xmpp_host;

    im_thread_t *signal_thread;
    im_thread_t *work_thread;

    im_conn_state_cb statecb;
    im_conn_recive_cb msgcb;
    void *userdata;

    im_conn_state state;
};

#endif // __IMCORE_INL_H__
