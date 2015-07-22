/* ctx.c
 * 上下文实现
 */
#include "xmpp-inl.h"

void xmpp_initialize(void)
{
#if defined(WIN32)
    evthread_use_windows_threads();
#endif
#if defined(POSIX)
    evthread_use_pthreads();
#endif

    // 加载socket库
    sock_initialize();

    // 加载SSL库
    SSL_library_init();
    ERR_load_crypto_strings();
    SSL_load_error_strings();

    if (RAND_poll() == 0) {
        fprintf(stderr, "RAND_poll() failed.\n");
    }
}

void xmpp_shutdown(void)
{
    // 关闭socket库
    sock_shutdown();
    // 清理event库 libevent2.1.xx新增
    libevent_global_shutdown();

}

static void *_malloc(const size_t size, void *userdata)
{
    return malloc(size);
}

static void _free(void *p, void *userdata)
{
    free(p);
}

static void *_realloc(void *p, size_t size, void * const userdata)
{
    return realloc(p, size);
}

// 内存管理对象
static xmpp_mem_t xmpp_default_mem = {
    _malloc,
    _free,
    _realloc,
    NULL
};

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


void *xmpp_alloc(const xmpp_ctx_t *ctx, size_t size)
{
    return ctx->mem->alloc(size, ctx->mem->userdata);
}

void xmpp_free(const xmpp_ctx_t *ctx, void *p)
{
    ctx->mem->free(p, ctx->mem->userdata);
}

void *xmpp_realloc(const xmpp_ctx_t *ctx, void *p, size_t size)
{
    return ctx->mem->realloc(p, size, ctx->mem->userdata);
}

void xmpp_log(const xmpp_ctx_t *ctx, xmpp_log_level_t level, const char *area, const char *fmt,
              va_list ap)
{
    int oldret, ret;
    char smbuf[1024];
    char *buf;
    va_list copy;

    va_copy(copy, ap);
    ret = imcore_vsnprintf(smbuf, sizeof(smbuf), fmt, ap);
    if (ret >= (int)sizeof(smbuf)) {
        buf = (char *)xmpp_alloc(ctx, ret + 1);
        if (!buf) {
            buf = NULL;
            xmpp_error(ctx, "log", "Failed allocating memory for log message.");
            va_end(copy);
            return;
        }
        oldret = ret;
        ret = imcore_vsnprintf(buf, ret + 1, fmt, copy);
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


xmpp_ctx_t *xmpp_ctx_new(const xmpp_mem_t *mem, const xmpp_log_t *log)
{
    xmpp_ctx_t *ctx = NULL;

    if (mem == NULL)
        ctx = xmpp_default_mem.alloc(sizeof(xmpp_ctx_t), NULL);
    else
        ctx = mem->alloc(sizeof(xmpp_ctx_t), mem->userdata);

    if (ctx != NULL) {
        if (mem != NULL)
            ctx->mem = mem;
        else
            ctx->mem = &xmpp_default_mem;

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

