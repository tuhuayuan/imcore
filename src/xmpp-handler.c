/* handler.c
 * 回调机制实现
 */
#include "xmpp-inl.h"

static inline bool _check_handler_exist(xmpp_handlist_t *list, void *handler)
{
    xmpp_handlist_t *pos_item = NULL;
    struct list_head *pos = NULL;

    if (list != NULL) {
        list_for_each(pos, &list->dlist) {
            pos_item = list_entry(pos, xmpp_handlist_t, dlist);
            if (pos_item->handler == handler) {
                return true;
            }
        }
    }
    return false;
}

static void _handler_timed_free(xmpp_handlist_t *target);
static void _handler_id_free(xmpp_handlist_t *head, xmpp_handlist_t *item, const char *id);
static void _handler_free(xmpp_handlist_t *target);


// 计时器回调代理
static void _handler_timer_proxy(evutil_socket_t fd, short what, void *arg)
{
    // 获取代理的句柄
    xmpp_handlist_t *item = arg;

    if (item->user_handler && !item->conn->authenticated) {
        if (!((xmpp_timed_handler)(item->user_handler))(item->conn, item->userdata)) {
            _handler_timed_free(item);
        }
    }
}

void handler_fire_stanza(xmpp_conn_t *conn, xmpp_stanza_t *stanza)
{
    xmpp_handlist_t *item = NULL, *pos_item = NULL;
    char *id, *ns, *name, *type;
    struct list_head *pos, *tmp;

    // 先处理id
    id = xmpp_stanza_get_id_ptr(stanza);
    if (id) {
        item = (xmpp_handlist_t*)hash_get(conn->id_handlers, id);
        if (item) {
            list_for_each_safe(pos, tmp, &item->dlist) {
                pos_item = list_entry(pos, xmpp_handlist_t, dlist);

                // 没有登录的话不调用用户的handler
                if (pos_item->user_handler && !conn->authenticated) {
                    continue;
                }
                if (!((xmpp_handler)(pos_item->handler))(conn, stanza, pos_item->userdata)) {
                    xmpp_id_handler_delete(conn, pos_item->handler, id);
                }
            }
        }
    }

    // handler派发信息, 回头会遍历并且匹配
    ns = xmpp_stanza_get_ns_ptr(stanza);
    name = xmpp_stanza_get_name_ptr(stanza);
    type = xmpp_stanza_get_type_ptr(stanza);

    // 普通handler
    item = &conn->handlers;
    if (!list_empty(&item->dlist)) {
        // 激活本次要运行的handler，之后加入的下一次事件激活的时候才处理
        list_for_each(pos, &item->dlist) {
            pos_item = list_entry(pos, xmpp_handlist_t, dlist);
            pos_item->enabled = 1;
        }

        // 激活回调
        list_for_each_safe(pos, tmp, &item->dlist) {
            pos_item = list_entry(pos, xmpp_handlist_t, dlist);

            // 没有登录的话不调用用户的handler
            if (!pos_item->enabled || (pos_item->user_handler && !conn->authenticated)) {
                continue;
            }
            // handler匹配条件: ns name type 如果有都要匹配
            if ((!pos_item->ns || (ns && strcmp(ns, pos_item->ns) == 0)
                 || xmpp_stanza_get_child_by_ns(stanza, pos_item->ns)) &&
                (!pos_item->name || (name && strcmp(name, pos_item->name) == 0)) &&
                (!pos_item->type || (type && strcmp(type, pos_item->type) == 0))) {
                if (!((xmpp_handler)(pos_item->handler))(conn, stanza, pos_item->userdata)) {
                    _handler_free(pos_item);
                }
            }
        } // end list_for_each_safe
    } // end if(list_empty?)
}

void handler_reset_timed(xmpp_conn_t *conn, int user_only)
{
    xmpp_handlist_t *item = NULL, *pos_item = NULL;
    struct list_head *pos;

    item = &conn->timed_handlers;
    list_for_each(pos, &item->dlist) {
        pos_item = list_entry(pos, xmpp_handlist_t, dlist);

        // 如果设置了user_only，则只重置外部的计时器
        if ((user_only && pos_item->user_handler) || !user_only) {
            evtimer_del(pos_item->evtimeout);
            evtimer_add(pos_item->evtimeout, &pos_item->period);
        }
    }
}

static void _timed_handler_add(xmpp_conn_t *conn,
                               xmpp_timed_handler handler,
                               unsigned long period,
                               void *userdata, int user_handler)
{
    xmpp_handlist_t *new_item = NULL, *head_item = NULL, *pos_item = NULL;
    struct list_head *pos = NULL;

    // handler唯一性
    head_item = &conn->timed_handlers;
    if (_check_handler_exist(head_item, handler)) {
        return;
    }

    // 分配
    new_item = xmpp_alloc(conn->ctx, sizeof(xmpp_handlist_t));
    if (!new_item)
        return;

    new_item->conn = conn;
    new_item->user_handler = user_handler;
    new_item->handler = (void*)handler;
    new_item->userdata = userdata;
    new_item->enabled = 0;

    // 设置定时器
    memset(&new_item->period, 0, sizeof(new_item->period));
    new_item->period.tv_sec = period;
    new_item->evtimeout = event_new(conn->ctx->base, -1, EV_TIMEOUT | EV_PERSIST, _handler_timer_proxy,
                                    new_item);
    if (!new_item->evtimeout) {
        xmpp_free(conn->ctx, new_item);
        return;
    }
    evtimer_add(new_item->evtimeout, &new_item->period);

    // 加入链表
    INIT_LIST_HEAD(&(new_item->dlist));
    list_add_tail(&new_item->dlist, &head_item->dlist);
}

void xmpp_timed_handler_delete(xmpp_conn_t *conn, xmpp_timed_handler handler)
{
    xmpp_handlist_t *head_item = NULL, *pos_item = NULL;
    struct list_head *pos = NULL, *tmp = NULL;

    head_item = &conn->timed_handlers;
    list_for_each_safe(pos, tmp, &head_item->dlist) {
        pos_item = list_entry(pos, xmpp_handlist_t, dlist);
        if (pos_item->handler == (void*)handler) {
            _handler_timed_free(pos_item);
            break;
        }
    }
}

static void _handler_timed_free(xmpp_handlist_t *target)
{
    list_del(&target->dlist);

    // 释放定时器
    evtimer_del(target->evtimeout);
    event_free(target->evtimeout);
    xmpp_free(target->conn->ctx, target);
}

static void _id_handler_add(xmpp_conn_t *conn,
                            xmpp_handler handler,
                            const char *id,
                            void *userdata,
                            int user_handler)
{
    xmpp_handlist_t *new_item = NULL, *head_item = NULL, *pos_item = NULL;
    struct list_head *pos = NULL;

    // handler唯一性
    head_item = (xmpp_handlist_t *)hash_get(conn->id_handlers, id);
    if (_check_handler_exist(head_item, handler)) {
        return;
    }

    // 分配新的handler
    new_item = xmpp_alloc(conn->ctx, sizeof(xmpp_handlist_t));
    if (!new_item) return;

    new_item->conn = conn;
    new_item->user_handler = user_handler;
    new_item->handler = (void *)handler;
    new_item->userdata = userdata;
    new_item->enabled = 0;
    INIT_LIST_HEAD(&new_item->dlist);

    // 复制id字符串
    new_item->id = xmpp_strdup(conn->ctx, id);
    if (!new_item->id) {
        xmpp_free(conn->ctx, new_item);
        return;
    }

    if (!head_item) {
        // 该id没有表头记，给他分配一个
        head_item = xmpp_alloc(conn->ctx, sizeof(xmpp_handlist_t));
        if (!head_item) {
            xmpp_free(conn->ctx, new_item);
            return;
        } else {
            // 添加id表头
            head_item->conn = conn;
            INIT_LIST_HEAD(&head_item->dlist);
            hash_add(conn->id_handlers, id, head_item);
        }
    }
    list_add_tail(&new_item->dlist, &head_item->dlist);
}

void xmpp_id_handler_delete(xmpp_conn_t *conn, xmpp_handler handler, const char *id)
{
    xmpp_handlist_t *head_item, *pos_item;
    struct list_head *pos;

    head_item = (xmpp_handlist_t *)hash_get(conn->id_handlers, id);
    if (!head_item) return;

    list_for_each(pos, &head_item->dlist) {
        pos_item = list_entry(pos, xmpp_handlist_t, dlist);

        if (pos_item->handler == (void*)handler) {
            _handler_id_free(head_item, pos_item, id);
            return;
        }
    }
}

static void _handler_id_free(xmpp_handlist_t *head, xmpp_handlist_t *item, const char *id)
{
    xmpp_conn_t *conn = head->conn;
    xmpp_handlist_t *pos_item;
    struct list_head *pos = NULL, *tmp = NULL;

    if (item == NULL) {
        list_for_each_safe(pos, tmp, &head->dlist) {
            pos_item = list_entry(pos, xmpp_handlist_t, dlist);
            _handler_id_free(head, pos_item, id);
        }
    } else {
        list_del(&item->dlist);

        if (list_empty(&head->dlist)) {
            hash_drop(conn->id_handlers, id);
            xmpp_free(conn->ctx, head);
        }

        xmpp_free(conn->ctx, item->id);
        xmpp_free(conn->ctx, item);
    }
}

static void _handler_add(xmpp_conn_t *conn, xmpp_handler handler, const char *ns,
                         const char *name,
                         const char *type, void *userdata, int user_handler)
{
    xmpp_handlist_t *new_item = NULL, *head_item = NULL, *pos_item = NULL;
    struct list_head *pos = NULL;

    // handler唯一性
    head_item = &conn->handlers;
    if (_check_handler_exist(head_item, handler)) {
        return;
    }

    new_item = (xmpp_handlist_t *)xmpp_alloc(conn->ctx, sizeof(xmpp_handlist_t));
    if (!new_item)
        return;

    new_item->conn = conn;
    new_item->user_handler = user_handler;
    new_item->handler = (void *)handler;
    new_item->userdata = userdata;
    new_item->enabled = 0;

    new_item->ns = NULL;
    new_item->name = NULL;
    new_item->type = NULL;

    if (ns) {
        new_item->ns = xmpp_strdup(conn->ctx, ns);
        if (!new_item->ns) {
            xmpp_free(conn->ctx, new_item);
            return;
        }
    }

    if (name) {
        new_item->name = xmpp_strdup(conn->ctx, name);
        if (!new_item->name) {
            if (new_item->ns) xmpp_free(conn->ctx, new_item->ns);
            xmpp_free(conn->ctx, new_item);
            return;
        }
    }

    if (type) {
        new_item->type = xmpp_strdup(conn->ctx, type);
        if (!new_item->type) {
            if (new_item->ns) xmpp_free(conn->ctx, new_item->ns);
            if (new_item->name) xmpp_free(conn->ctx, new_item->name);
            xmpp_free(conn->ctx, new_item);
            return;
        }
    }

    // 入表
    head_item = &conn->handlers;
    INIT_LIST_HEAD(&new_item->dlist);
    list_add_tail(&new_item->dlist, &head_item->dlist);
}

void xmpp_handler_delete(xmpp_conn_t *conn, xmpp_handler handler)
{
    xmpp_handlist_t *head_item = NULL, *pos_item = NULL;
    struct list_head *pos = NULL, *tmp = NULL;

    head_item = &conn->handlers;
    list_for_each_safe(pos, tmp, &head_item->dlist) {
        pos_item = list_entry(pos, xmpp_handlist_t, dlist);

        if (pos_item->handler == (void*)handler) {
            _handler_free(pos_item);
            return;
        }
    }
}

static void _handler_free(xmpp_handlist_t *target)
{
    xmpp_ctx_t *ctx = target->conn->ctx;
    list_del(&target->dlist);

    // 释放分配的值
    if (target->ns)
        xmpp_free(ctx, target->ns);
    if (target->name)
        xmpp_free(ctx, target->name);
    if (target->type)
        xmpp_free(ctx, target->type);
    xmpp_free(ctx, target);
}


void handler_clear_all(xmpp_conn_t *conn)
{
    xmpp_ctx_t *ctx = NULL;
    xmpp_handlist_t *head_item = NULL, *pos_item = NULL;
    struct list_head *pos = NULL, *tmp = NULL;
    hash_iterator_t *iter = NULL;
    const char *key = NULL;

    // 释放定时器
    head_item = &conn->timed_handlers;
    list_for_each_safe(pos, tmp, &head_item->dlist) {
        pos_item = list_entry(pos, xmpp_handlist_t, dlist);
        _handler_timed_free(pos_item);
    }

    // 释放所有的IDhandler, 以及hash表
    iter = hash_iter_new(conn->id_handlers);
    while ((key = hash_iter_next(iter))) {
        head_item = (xmpp_handlist_t *)hash_get(conn->id_handlers, key);
        _handler_id_free(head_item, NULL, key);
    }
    hash_iter_release(iter);
    hash_release(conn->id_handlers);

    // 释放所有handler
    head_item = &conn->handlers;
    list_for_each_safe(pos, tmp, &head_item->dlist) {
        pos_item = list_entry(pos, xmpp_handlist_t, dlist);
        _handler_free(pos_item);
    }
}

void xmpp_timed_handler_add(xmpp_conn_t *conn, xmpp_timed_handler handler,
                            unsigned long period,
                            void *userdata)
{
    _timed_handler_add(conn, handler, period, userdata, 1);
}

void handler_add_timed(xmpp_conn_t *conn,  xmpp_timed_handler handler,
                       unsigned long period,
                       void *userdata)
{
    _timed_handler_add(conn, handler, period, userdata, 0);
}

void xmpp_id_handler_add(xmpp_conn_t *conn, xmpp_handler handler, const char *id,
                         void *userdata)
{
    _id_handler_add(conn, handler, id, userdata, 1);
}

void handler_add_id(xmpp_conn_t *conn, xmpp_handler handler, const char *id,
                    void *userdata)
{
    _id_handler_add(conn, handler, id, userdata, 0);
}

void xmpp_handler_add(xmpp_conn_t *conn, xmpp_handler handler, const char *ns,
                      const char *name,
                      const char *type, void *userdata)
{
    _handler_add(conn, handler, ns, name, type, userdata, 1);
}

void handler_add(xmpp_conn_t *conn, xmpp_handler handler, const char *ns,
                 const char *name,
                 const char *type, void *userdata)
{
    _handler_add(conn, handler, ns, name, type, userdata, 0);
}
