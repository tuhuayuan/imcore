/************************************************************************/
/* common.h 通用方法、结构体                                             */
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

// 内存管理
#define xmpp_alloc(userdata, size) (safe_mem_malloc(size, userdata))
#define xmpp_realloc(userdata, p, size) (safe_mem_realloc(p, size, userdata))
#define xmpp_free(unused, p) (safe_mem_free(p))

// 运行状态
typedef enum {
    XMPP_LOOP_NOTSTARTED,
    XMPP_LOOP_RUNNING,
    XMPP_LOOP_QUIT
} xmpp_loop_status_t;

// xmpp运行上下文对象
struct _xmpp_ctx_t {
    xmpp_loop_status_t loop_status;    // 事件循环状态
    struct event_base *base;           // 事件循环
    SSL_CTX *ssl_ctx;                  // ssl上下文环境
    const xmpp_log_t *log;             // 日志管理
};

//日志管理helper
void xmpp_log(const xmpp_ctx_t *ctx, xmpp_log_level_t level, const char *area,
              const char *fmt, va_list ap);
void xmpp_error(const xmpp_ctx_t *ctx, const char *area, const char *fmt, ...);
void xmpp_warn(const xmpp_ctx_t *ctx, const char *area, const char *fmt, ...);
void xmpp_info(const xmpp_ctx_t *ctx, const char *area, const char *fmt, ...);
void xmpp_debug(const xmpp_ctx_t *ctx, const char *area, const char *fmt, ...);


// 字符串复制
char *xmpp_strdup(const xmpp_ctx_t *ctx, const char *s);

// JID helper
char *xmpp_jid_new(xmpp_ctx_t *ctx, const char *node, const char *domain,
                   const char *resource);
char *xmpp_jid_bare(xmpp_ctx_t *ctx, const char *jid);
char *xmpp_jid_node(xmpp_ctx_t *ctx, const char *jid);
char *xmpp_jid_domain(xmpp_ctx_t *ctx, const char *id);
char *xmpp_jid_resource(xmpp_ctx_t *ctx, const char *jid);

// 连接状态
typedef enum {
    XMPP_STATE_DISCONNECTED,
    XMPP_STATE_CONNECTING,
    XMPP_STATE_CONNECTED
} xmpp_conn_state_t;

// Handler链表
typedef struct _xmpp_handlist_t xmpp_handlist_t;
struct _xmpp_handlist_t {
    int user_handler;                     // 是否是用户handler
    xmpp_conn_t *conn;
    void *handler;
    void *userdata;
    int enabled;

    // 链表头
    struct list_head dlist;

    union {
        /* timed handlers */
        struct {
            struct timeval period;           // 触发间隔
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

// 支持的SASL认证方式
#define SASL_MASK_PLAIN 0x01
#define SASL_MASK_DIGESTMD5 0x02

// stream流开启的回调函数签名
typedef void (*xmpp_open_handler)(xmpp_conn_t *const conn);

struct _xmpp_conn_t {
    unsigned int ref;                     // 引用计数
    xmpp_ctx_t *ctx;                      // 所属上下文
    xmpp_conn_type_t type;
    xmpp_conn_state_t state;
    void *userdata;

    int error;
    unsigned long respond_timeout;         // 超时限制
    xmpp_stream_error_t *stream_error;     // 最后的错误对象
    struct bufferevent *evbuffer;

    int tls_disabled;                     // 客户端是否允许tls
    int tls_support;                      // 是否支持tls
    int sasl_support;                     // 支持什么sasl
    int zlib_support;                     // 支持zlib压缩否
    int secured;                          // 是否是安全连接
    int tls_failed;                       // 建立tls失败了
    int bind_required;                    // 服务器强制要求绑定资源
    int session_required;                 // 服务器强制要求绑定session

    // Xmpp信息
    char *lang;
    char *domain;
    char *connectdomain;
    char *connectport;
    char *jid;
    char *pass;
    char *bound_jid;
    char *stream_id;
    int authenticated;                    // 是否已经完成握手

    // xmpp stanza 解析器
    parser_t *parser;

    // auth
    xmpp_open_handler open_handler;

    // handles
    xmpp_handlist_t timed_handlers;
    xmpp_handlist_t handlers;
    hash_t *id_handlers;


    // 连接回调函数（外部接口）
    xmpp_conn_handler conn_handler;
};

// 直接断开
void conn_do_disconnect(xmpp_conn_t *conn);

// 发送流初始化
void conn_init_stream(xmpp_conn_t *conn);

// 启动ssl
void conn_start_ssl(xmpp_conn_t *conn);

// 启用zlib压缩
int conn_start_compression(xmpp_conn_t *conn);

// 设置xmpp_open_handler并且重置解析器
void conn_reset_stream(xmpp_conn_t *conn, xmpp_open_handler handler);

// xmpp stanza类型
typedef enum {
    XMPP_STANZA_UNKNOWN,
    XMPP_STANZA_TEXT,
    XMPP_STANZA_TAG
} xmpp_stanza_type_t;

// xmpp stanza对象
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

// 触发stanza回调
void handler_fire_stanza(xmpp_conn_t *conn, xmpp_stanza_t *stanza);

// 所有定时器handler重新开始计事件
void handler_reset_timed(xmpp_conn_t *conn, int user_only);

// 内部handler管理
void handler_add_timed(xmpp_conn_t *conn, xmpp_timed_handler handler,
                       unsigned long period,
                       void *userdata);
void handler_add_id(xmpp_conn_t *conn, xmpp_handler handler, const char *id,
                    void *userdata);
void handler_add(xmpp_conn_t *conn, xmpp_handler handler, const char *ns,
                 const char *name,
                 const char *type, void *userdata);
void handler_clear_all(xmpp_conn_t *conn);

// 连接建立，处理stanza流入口
void auth_handle_open(xmpp_conn_t *conn);

// hash释放回调
void util_hash_free(const xmpp_ctx_t* const ctx, void* p);
#endif /* __IMCORE_XMPP_COMMON_H__ */
