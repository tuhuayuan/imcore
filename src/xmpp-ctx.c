/* ctx.c
 * 上下文实现
 */
#include "xmpp-inl.h"

void xmpp_initialize(void)
{
}

void xmpp_shutdown(void)
{
}

// 日志等级
static const char * const _xmpp_log_level_name[4] = {"DEBUG", "INFO", "WARN", "ERROR"};
static const xmpp_log_level_t _xmpp_default_logger_levels[] = {XMPP_LEVEL_DEBUG,
                                                               XMPP_LEVEL_INFO,
                                                               XMPP_LEVEL_WARN,
                                                               XMPP_LEVEL_ERROR
                                                              };
void xmpp_default_logger(void *userdata, xmpp_log_level_t level, const char *area,
                         const char *msg)
{
    xmpp_log_level_t filter_level = * (xmpp_log_level_t*)userdata;
    if (level >= filter_level)
        fprintf(stderr, "%s %s %s\n", area, _xmpp_log_level_name[level], msg);
}

static const xmpp_log_t _xmpp_default_loggers[] = {
    {&xmpp_default_logger, (void*)&_xmpp_default_logger_levels[XMPP_LEVEL_DEBUG]},
    {&xmpp_default_logger, (void*)&_xmpp_default_logger_levels[XMPP_LEVEL_INFO]},
    {&xmpp_default_logger, (void*)&_xmpp_default_logger_levels[XMPP_LEVEL_WARN]},
    {&xmpp_default_logger, (void*)&_xmpp_default_logger_levels[XMPP_LEVEL_ERROR]}
};

xmpp_log_t *xmpp_get_default_logger(xmpp_log_level_t level)
{
    if (level > XMPP_LEVEL_ERROR) level = XMPP_LEVEL_ERROR;
    if (level < XMPP_LEVEL_DEBUG) level = XMPP_LEVEL_DEBUG;
    
    return (xmpp_log_t*)&_xmpp_default_loggers[level];
}
static xmpp_log_t xmpp_default_log = { NULL, NULL };

void xmpp_log(const xmpp_ctx_t *ctx, xmpp_log_level_t level, const char *area, const char *fmt,
              va_list ap)
{
    int oldret, ret;
    char smbuf[1024];
    char *buf;
    va_list copy;
    
    va_copy(copy, ap);
    ret = im_vsnprintf(smbuf, sizeof(smbuf), fmt, ap);
    if (ret >= (int)sizeof(smbuf)) {
        buf = (char*)xmpp_alloc((void*)ctx, ret + 1);
        if (!buf) {
            buf = NULL;
            xmpp_error(ctx, "log", "Failed allocating memory for log message.");
            va_end(copy);
            return;
        }
        oldret = ret;
        ret = im_vsnprintf(buf, ret + 1, fmt, copy);
        if (ret > oldret) {
            xmpp_error(ctx, "log", "Unexpected error");
            xmpp_free(ctx, buf);
            va_end(copy);
            return;
        }
    } else {
        buf = smbuf;
    }
    va_end(copy);
    
    if (ctx->log->handler)
        ctx->log->handler(ctx->log->userdata, level, area, buf);
        
    if (buf != smbuf)
        xmpp_free(ctx, buf);
}


void xmpp_error(const xmpp_ctx_t *ctx, const char *area, const char *fmt, ...)
{
    va_list ap;
    
    va_start(ap, fmt);
    xmpp_log(ctx, XMPP_LEVEL_ERROR, area, fmt, ap);
    va_end(ap);
}

void xmpp_warn(const xmpp_ctx_t *ctx, const char *area, const char *fmt, ...)
{
    va_list ap;
    
    va_start(ap, fmt);
    xmpp_log(ctx, XMPP_LEVEL_WARN, area, fmt, ap);
    va_end(ap);
}

void xmpp_info(const xmpp_ctx_t *ctx, const char *area, const char *fmt, ...)
{
    va_list ap;
    
    va_start(ap, fmt);
    xmpp_log(ctx, XMPP_LEVEL_INFO, area, fmt, ap);
    va_end(ap);
}

void xmpp_debug(const xmpp_ctx_t *ctx, const char *area, const char *fmt, ...)
{
    va_list ap;
    
    va_start(ap, fmt);
    xmpp_log(ctx, XMPP_LEVEL_DEBUG, area, fmt, ap);
    va_end(ap);
}


xmpp_ctx_t *xmpp_ctx_new(const xmpp_log_t *log)
{
    xmpp_ctx_t *ctx = NULL;
    ctx = safe_mem_malloc(sizeof(xmpp_ctx_t), NULL);
    
    if (ctx != NULL) {
        if (log == NULL)
            ctx->log = &xmpp_default_log;
        else
            ctx->log = log;
            
        // 初始化eventbase
        ctx->base = event_base_new();
        
        // 初始化SSL上下文
        ctx->ssl_ctx = SSL_CTX_new(TLS_client_method());
        ctx->loop_status = XMPP_LOOP_NOTSTARTED;
    }
    
    return ctx;
}

void xmpp_ctx_free(xmpp_ctx_t *ctx)
{
    event_base_free(ctx->base);
    SSL_CTX_free(ctx->ssl_ctx);
    xmpp_free(ctx, ctx);
}

void xmpp_hash_free(void* p)
{
    safe_mem_free(p);
}
