/************************************************************************/
/* common.h ͨ�÷������ṹ��                                             */
/************************************************************************/

#ifndef __IMCORE_XMPP_COMMON_H__
#define __IMCORE_XMPP_COMMON_H__

#include <event2/event.h>
#include <event2/thread.h>
#include <event2/util.h>
#include <event2/bufferevent.h>
#include <event2/bufferevent_ssl.h>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>

#include "common.h"
#include "list.h"
#include "sock.h"
#include "mm.h"

#include "xmpp.h"
#include "xmpp-hash.h"
#include "xmpp-parser.h"
#include "xmpp-sasl.h"

// �ڴ����
#define xmpp_alloc(userdata, size) (safe_mem_malloc(size, userdata))
#define xmpp_realloc(userdata, p, size) (safe_mem_realloc(p, size, userdata))
#define xmpp_free(unused, p) (safe_mem_free(p))

// ����״̬
typedef enum {
    XMPP_LOOP_NOTSTARTED,
    XMPP_LOOP_RUNNING,
    XMPP_LOOP_QUIT
} xmpp_loop_status_t;

// xmpp���������Ķ���
struct _xmpp_ctx_t {
    xmpp_loop_status_t loop_status;    // �¼�ѭ��״̬
    struct event_base *base;           // �¼�ѭ��
    SSL_CTX *ssl_ctx;                  // ssl�����Ļ���
    const xmpp_log_t *log;             // ��־����
};

//��־����helper
void xmpp_log(const xmpp_ctx_t *ctx, xmpp_log_level_t level, const char *area,
              const char *fmt, va_list ap);
void xmpp_error(const xmpp_ctx_t *ctx, const char *area, const char *fmt, ...);
void xmpp_warn(const xmpp_ctx_t *ctx, const char *area, const char *fmt, ...);
void xmpp_info(const xmpp_ctx_t *ctx, const char *area, const char *fmt, ...);
void xmpp_debug(const xmpp_ctx_t *ctx, const char *area, const char *fmt, ...);


// �ַ�������
char *xmpp_strdup(const xmpp_ctx_t *ctx, const char *s);

// JID helper
char *xmpp_jid_new(xmpp_ctx_t *ctx, const char *node, const char *domain,
                   const char *resource);
char *xmpp_jid_bare(xmpp_ctx_t *ctx, const char *jid);
char *xmpp_jid_node(xmpp_ctx_t *ctx, const char *jid);
char *xmpp_jid_domain(xmpp_ctx_t *ctx, const char *id);
char *xmpp_jid_resource(xmpp_ctx_t *ctx, const char *jid);

// ����״̬
typedef enum {
    XMPP_STATE_DISCONNECTED,
    XMPP_STATE_CONNECTING,
    XMPP_STATE_CONNECTED
} xmpp_conn_state_t;

// Handler����
typedef struct _xmpp_handlist_t xmpp_handlist_t;
struct _xmpp_handlist_t {
    int user_handler;                     // �Ƿ����û�handler
    xmpp_conn_t *conn;
    void *handler;
    void *userdata;
    int enabled;

    // ����ͷ
    struct list_head dlist;

    union {
        /* timed handlers */
        struct {
            struct timeval period;           // �������
            struct event *evtimeout;
        };
        /* id handlers */
        struct {
            char *id;
        };
        /* normal handlers */
        struct {
            char *ns;
            char *name;
            char *type;
        };
    };
};

// ֧�ֵ�SASL��֤��ʽ
#define SASL_MASK_PLAIN 0x01
#define SASL_MASK_DIGESTMD5 0x02

// stream�������Ļص�����ǩ��
typedef void (*xmpp_open_handler)(xmpp_conn_t *const conn);

struct _xmpp_conn_t {
    unsigned int ref;                     // ���ü���
    xmpp_ctx_t *ctx;                      // ����������
    xmpp_conn_type_t type;
    xmpp_conn_state_t state;
    void *userdata;

    int error;
    unsigned long respond_timeout;         // ��ʱ����
    xmpp_stream_error_t *stream_error;     // ���Ĵ������
    struct bufferevent *evbuffer;

    int tls_disabled;                     // �ͻ����Ƿ�����tls
    int tls_support;                      // �Ƿ�֧��tls
    int sasl_support;                     // ֧��ʲôsasl
    int zlib_support;                     // ֧��zlibѹ����
    int secured;                          // �Ƿ��ǰ�ȫ����
    int tls_failed;                       // ����tlsʧ����
    int bind_required;                    // ������ǿ��Ҫ�����Դ
    int session_required;                 // ������ǿ��Ҫ���session

    // Xmpp��Ϣ
    char *lang;
    char *domain;
    char *connectdomain;
    char *connectport;
    char *jid;
    char *pass;
    char *bound_jid;
    char *stream_id;
    int authenticated;                    // �Ƿ��Ѿ��������

    // xmpp stanza ������
    parser_t *parser;

    // auth
    xmpp_open_handler open_handler;

    // handles
    xmpp_handlist_t timed_handlers;
    xmpp_handlist_t handlers;
    hash_t *id_handlers;


    // ���ӻص��������ⲿ�ӿڣ�
    xmpp_conn_handler conn_handler;
};

// ֱ�ӶϿ�
void conn_do_disconnect(xmpp_conn_t *conn);

// ��������ʼ��
void conn_init_stream(xmpp_conn_t *conn);

// ����ssl
void conn_start_ssl(xmpp_conn_t *conn);

// ����zlibѹ��
int conn_start_compression(xmpp_conn_t *conn);

// ����xmpp_open_handler�������ý�����
void conn_reset_stream(xmpp_conn_t *conn, xmpp_open_handler handler);

// xmpp stanza����
typedef enum {
    XMPP_STANZA_UNKNOWN,
    XMPP_STANZA_TEXT,
    XMPP_STANZA_TAG
} xmpp_stanza_type_t;

// xmpp stanza����
struct _xmpp_stanza_t {
    int ref;
    xmpp_ctx_t *ctx;
    xmpp_stanza_type_t type;

    xmpp_stanza_t *prev;
    xmpp_stanza_t *next;
    xmpp_stanza_t *children;
    xmpp_stanza_t *parent;

    char *data;
    hash_t *attributes;
};

// ����stanza�ص�
void handler_fire_stanza(xmpp_conn_t *conn, xmpp_stanza_t *stanza);

// ���ж�ʱ��handler���¿�ʼ���¼�
void handler_reset_timed(xmpp_conn_t *conn, int user_only);

// �ڲ�handler����
void handler_add_timed(xmpp_conn_t *conn, xmpp_timed_handler handler,
                       unsigned long period,
                       void *userdata);
void handler_add_id(xmpp_conn_t *conn, xmpp_handler handler, const char *id,
                    void *userdata);
void handler_add(xmpp_conn_t *conn, xmpp_handler handler, const char *ns,
                 const char *name,
                 const char *type, void *userdata);
void handler_clear_all(xmpp_conn_t *conn);

// ���ӽ���������stanza�����
void auth_handle_open(xmpp_conn_t *conn);

// hash�ͷŻص�
void util_hash_free(const xmpp_ctx_t* const ctx, void* p);
#endif /* __IMCORE_XMPP_COMMON_H__ */
