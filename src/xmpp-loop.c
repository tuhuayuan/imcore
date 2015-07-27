/* event.c
 * 事件驱动
 */
#include "xmpp-inl.h"

void xmpp_run_once(xmpp_ctx_t *ctx, unsigned long timeout)
{
    if (ctx->loop_status == XMPP_LOOP_QUIT)
        return;
        
    if (timeout > 0) {
        struct timeval t;
        t.tv_sec = 0;
        t.tv_usec = timeout;
        event_base_loopexit(ctx->base, &t);
    } else {
        event_base_loop(ctx->base, EVLOOP_ONCE);
    }
}

void xmpp_run(xmpp_ctx_t *ctx)
{
    if (ctx->loop_status != XMPP_LOOP_NOTSTARTED)
        return;
        
    ctx->loop_status = XMPP_LOOP_RUNNING;
    while (ctx->loop_status == XMPP_LOOP_RUNNING) {
        xmpp_run_once(ctx, 0);
    }
    
    xmpp_debug(ctx, "event", "Event loop completed.");
}

void xmpp_stop(xmpp_ctx_t *ctx)
{
    xmpp_debug(ctx, "event", "Stopping event loop.");
    
    if (ctx->loop_status == XMPP_LOOP_RUNNING)
        ctx->loop_status = XMPP_LOOP_QUIT;
}
