/**
 * @file    src\im-msg-text.c
 *
 * @brief   实现文本消息发送
 */
#include "im-msg-text.h"
#include "im-inl.h"
#include "xmpp-msg.h"

#define IM_TEXT_MSG_TYPE		"text"
#define IM_TEXT_MSG_BODY_MAX	1024

static void _im_msg_text_free(im_msg_t *msg);
static void _im_msg_text_send(im_msg_t *msg, im_conn_send_cb cb, bool receipt, void *userdata);

struct im_msg_text {
    im_msg_t common_msg;		// 抽象消息排在头部,方便指针转换
    char body[IM_TEXT_MSG_BODY_MAX];
    size_t len;
    int require;
};
typedef struct im_msg_text im_msg_text_t;

int msg_text_handler(xmpp_conn_t *xmpp_conn, xmpp_stanza_t *stanza, void *userdata)
{
    im_conn_t *conn = (im_conn_t*)userdata;
    im_msg_text_t *text_msg = NULL;

    if (!xmpp_msg_valid(stanza))
        return -1;

    char *type;
    type = xmpp_msg_get_type(stanza);
    // 目前只能处理单聊
    if (im_strcmp(type, "chat") != 0)
        return -1;

    text_msg = safe_mem_calloc(sizeof(struct im_msg_text), NULL);
    if (!text_msg)
        return -1;

    // stanza里面的数据都会以拷贝的形式复制到im_msg结构体
    int ret = im_msg_init(&text_msg->common_msg, conn,
                          xmpp_msg_get_from(stanza), xmpp_msg_get_to(stanza),
                          IM_TEXT_MSG_TYPE,
                          _im_msg_text_free, NULL);
    if (ret != 0) {
        safe_mem_free(text_msg);
        return -1;
    }

    // 读取文本消息body消息为空也没有关系超过最大长度则截断
    ret = xmpp_msg_get_body(stanza, text_msg->body, IM_TEXT_MSG_BODY_MAX);
    if (ret >= 0)
        text_msg->len = ret;

    // 分发消息
    conn->msgcb(conn, &text_msg->common_msg, conn->userdata);

    // 释放消息
    im_msg_free(&text_msg->common_msg);

    return 0;
}

void _im_msg_text_free(im_msg_t *msg)
{
    // 释放抽象消息里面的分配
    im_msg_destroy(msg);
    // 文本消息结构体把抽象消息放在首部所以不需要计算偏移
    safe_mem_free(msg);
}

void _im_msg_text_send(im_msg_t *msg, im_conn_send_cb cb, bool receipt, void *userdata)
{
    im_msg_text_t *text_msg = (im_msg_text_t *)msg;
    xmpp_stanza_t *msg_stanza = NULL;

    if (im_strcmp(msg->type, IM_TEXT_MSG_TYPE))
        return;

    msg_stanza = xmpp_msg_create(msg->id, msg->to, msg->from, msg->type, (char*)text_msg->body);
}
