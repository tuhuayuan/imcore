#ifndef __IMCORE_XMPP_H__
#define __IMCORE_XMPP_H__

#include <stdio.h>
#include "im-thread.h"

#ifdef __cplusplus
extern "C" {
#endif

// 名字空间定义
#define XMPP_NS_CLIENT "jabber:client"
#define XMPP_NS_COMPONENT "jabber:component:accept"
#define XMPP_NS_STREAMS "http://etherx.jabber.org/streams"
#define XMPP_NS_STREAMS_IETF "urn:ietf:params:xml:ns:xmpp-streams"
#define XMPP_NS_TLS "urn:ietf:params:xml:ns:xmpp-tls"
#define XMPP_NS_SASL "urn:ietf:params:xml:ns:xmpp-sasl"
#define XMPP_NS_COMPRESSION "http://jabber.org/features/compress"
#define XMPP_NS_BIND "urn:ietf:params:xml:ns:xmpp-bind"
#define XMPP_NS_SESSION "urn:ietf:params:xml:ns:xmpp-session"
#define XMPP_NS_AUTH "jabber:iq:auth"
#define XMPP_NS_DISCO_INFO "http://jabber.org/protocol/disco#info"
#define XMPP_NS_DISCO_ITEMS "http://jabber.org/protocol/disco#items"
#define XMPP_NS_ROSTER "jabber:iq:roster"

// 错误定义
#define XMPP_EOK 0
#define XMPP_EMEM -1
#define XMPP_EINVOP -2
#define XMPP_EINT -3

// 初始化以及反初始化
void xmpp_initialize(void);
void xmpp_shutdown(void);

// 日志等级
typedef enum {
    XMPP_LEVEL_DEBUG,
    XMPP_LEVEL_INFO,
    XMPP_LEVEL_WARN,
    XMPP_LEVEL_ERROR
} xmpp_log_level_t;

// 日志处理回掉签名
typedef void(*xmpp_log_handler)(void *userdata, const xmpp_log_level_t level, const char *area,
                                const char *msg);
// 日志对象
struct _xmpp_log_t {
    xmpp_log_handler handler;
    void *userdata;
};
typedef struct _xmpp_log_t xmpp_log_t;

xmpp_log_t *xmpp_get_default_logger(xmpp_log_level_t level);

// 上下文对象
typedef struct _xmpp_ctx_t xmpp_ctx_t;
xmpp_ctx_t *xmpp_ctx_new(im_thread_t *workthread, const xmpp_log_t *log);
void xmpp_ctx_free(xmpp_ctx_t *ctx);

//连接类型
typedef enum {
    XMPP_UNKNOWN,
    XMPP_CLIENT
} xmpp_conn_type_t;

/* 预定义结构体 */
typedef struct _xmpp_conn_t xmpp_conn_t;
typedef struct _xmpp_stanza_t xmpp_stanza_t;

// xmpp连接状态定义
typedef enum {
    XMPP_CONN_CONNECT,
    XMPP_CONN_DISCONNECT,
    XMPP_CONN_FAIL
} xmpp_conn_event_t;

// xmpp 错误定义
typedef enum {
    XMPP_SE_BAD_FORMAT,
    XMPP_SE_BAD_NS_PREFIX,
    XMPP_SE_CONFLICT,
    XMPP_SE_CONN_TIMEOUT,
    XMPP_SE_HOST_GONE,
    XMPP_SE_HOST_UNKNOWN,
    XMPP_SE_IMPROPER_ADDR,
    XMPP_SE_INTERNAL_SERVER_ERROR,
    XMPP_SE_INVALID_FROM,
    XMPP_SE_INVALID_ID,
    XMPP_SE_INVALID_NS,
    XMPP_SE_INVALID_XML,
    XMPP_SE_NOT_AUTHORIZED,
    XMPP_SE_POLICY_VIOLATION,
    XMPP_SE_REMOTE_CONN_FAILED,
    XMPP_SE_RESOURCE_CONSTRAINT,
    XMPP_SE_RESTRICTED_XML,
    XMPP_SE_SEE_OTHER_HOST,
    XMPP_SE_SYSTEM_SHUTDOWN,
    XMPP_SE_UNDEFINED_CONDITION,
    XMPP_SE_UNSUPPORTED_ENCODING,
    XMPP_SE_UNSUPPORTED_STANZA_TYPE,
    XMPP_SE_UNSUPPORTED_VERSION,
    XMPP_SE_XML_NOT_WELL_FORMED
} xmpp_error_type_t;

// 流错误包含错误类型以及错误的xml节以及错误文字
typedef struct {
    xmpp_error_type_t type;
    char *text;
    xmpp_stanza_t *stanza;
} xmpp_stream_error_t;


// 生存周期管理
xmpp_conn_t *xmpp_conn_new(xmpp_ctx_t *ctx);
xmpp_conn_t *xmpp_conn_clone(xmpp_conn_t *conn);
int xmpp_conn_release(xmpp_conn_t *conn);

// 属性访问
xmpp_ctx_t* xmpp_conn_get_context(xmpp_conn_t *conn);
const char *xmpp_conn_get_jid(const xmpp_conn_t *conn);
const char *xmpp_conn_get_bound_jid(const xmpp_conn_t *conn);
void xmpp_conn_set_jid(xmpp_conn_t *conn, const char *jid);
const char *xmpp_conn_get_pass(const xmpp_conn_t *conn);
void xmpp_conn_set_timeout(xmpp_conn_t *conn, unsigned long usec);
void xmpp_conn_set_pass(xmpp_conn_t *conn, const char *pass);
void xmpp_conn_disable_tls(xmpp_conn_t *conn);

// 连接状态回调
typedef void(*xmpp_conn_handler)(xmpp_conn_t *conn,
                                 xmpp_conn_event_t state, int error,
                                 xmpp_stream_error_t *stream_error,
                                 void *userdata);

// 连接管理
int xmpp_connect_client(xmpp_conn_t *conn,
                        const char *altdomain,
                        unsigned short altport,
                        xmpp_conn_handler callback,
                        void *userdata);
void xmpp_disconnect(xmpp_conn_t *conn);

// 消息发送
void xmpp_send(xmpp_conn_t *conn, xmpp_stanza_t *stanza);
void xmpp_send_raw_string(xmpp_conn_t *conn, const char *fmt, ...);
void xmpp_send_raw(xmpp_conn_t *conn, const char *data, size_t len);

// handle回调
typedef int (*xmpp_timed_handler)(xmpp_conn_t *conn, void *userdata);
typedef int (*xmpp_handler)(xmpp_conn_t *conn, xmpp_stanza_t *stanza, void *userdata);

// handle回调返回值
#define XMPP_HANDLER_END      0
#define XMPP_HANDLER_AGAIN    1

// handle管理
void xmpp_timed_handler_add(xmpp_conn_t *conn,
                            xmpp_timed_handler handler,
                            unsigned long period,
                            void *userdata);
void xmpp_timed_handler_delete(xmpp_conn_t *conn, xmpp_timed_handler handler);
void xmpp_handler_add(xmpp_conn_t *conn,
                      xmpp_handler handler,
                      const char *ns,
                      const char *name,
                      const char *type,
                      void *userdata);
void xmpp_handler_delete(xmpp_conn_t *conn, xmpp_handler handler);
void xmpp_id_handler_add(xmpp_conn_t *conn, xmpp_handler handler, const char *id, void *userdata);
void xmpp_id_handler_delete(xmpp_conn_t *conn, xmpp_handler handler, const char *id);

// Stanza操作
xmpp_stanza_t *xmpp_stanza_new(xmpp_ctx_t *ctx);
xmpp_stanza_t *xmpp_stanza_clone(xmpp_stanza_t *stanza);
xmpp_stanza_t * xmpp_stanza_copy(const xmpp_stanza_t *stanza);
int xmpp_stanza_release(xmpp_stanza_t *stanza);
int xmpp_stanza_is_text(xmpp_stanza_t *stanza);
int xmpp_stanza_is_tag(xmpp_stanza_t *stanza);
int xmpp_stanza_to_text(xmpp_stanza_t *stanza, char **const buf, size_t *buflen);
xmpp_stanza_t *xmpp_stanza_get_children(xmpp_stanza_t *stanza);
xmpp_stanza_t *xmpp_stanza_get_child_by_name(xmpp_stanza_t *stanza, const char *name);
xmpp_stanza_t *xmpp_stanza_get_child_by_ns(xmpp_stanza_t *stanza, const char *ns);
xmpp_stanza_t *xmpp_stanza_get_next(xmpp_stanza_t *stanza);
const char *xmpp_stanza_get_attribute(xmpp_stanza_t *stanza, const char *name);
const char *xmpp_stanza_get_ns(xmpp_stanza_t *stanza);
char *xmpp_stanza_get_text_ptr(xmpp_stanza_t *stanza);
char *xmpp_stanza_get_name_ptr(xmpp_stanza_t *stanza);
int xmpp_stanza_add_child(xmpp_stanza_t *stanza, xmpp_stanza_t *child);
int xmpp_stanza_set_ns(xmpp_stanza_t *stanza, const char *ns);
int xmpp_stanza_set_attribute(xmpp_stanza_t *stanza, const char *key, const char *value);
int xmpp_stanza_set_name(xmpp_stanza_t *stanza, const char *name);
int xmpp_stanza_set_text(xmpp_stanza_t *stanza, const char *text);
int xmpp_stanza_set_text_safe(xmpp_stanza_t *stanza, const char *text, size_t size);
char *xmpp_stanza_get_type_ptr(xmpp_stanza_t *stanza);
char *xmpp_stanza_get_id_ptr(xmpp_stanza_t *stanza);
int xmpp_stanza_set_id(xmpp_stanza_t *stanza, const char *id);
int xmpp_stanza_set_type(xmpp_stanza_t *stanza, const char *type);

// 循环控制
void xmpp_run_once(xmpp_ctx_t *ctx, unsigned long timeout);
void xmpp_run(xmpp_ctx_t *ctx);
void xmpp_stop(xmpp_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif // __IMCORE_XMPP_H__
