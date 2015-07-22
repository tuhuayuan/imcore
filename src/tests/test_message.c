#include "test_message.h"
#include "xmpp.h"

#include "iconv.h"

typedef struct im_text_msg {
    im_msg_t common_info;             // 基础消息信息
    char *txt;                        // 文本信息
    char *type;
    
} im_text_msg_t;

static xmpp_stanza_t *txt_msg_to_stanza(im_text_msg_t *txt_msg, xmpp_conn_t *conn)
{
    xmpp_ctx_t *ctx = xmpp_conn_get_context(conn);
    xmpp_stanza_t *msg_stanza = xmpp_stanza_new(ctx);
    
    xmpp_stanza_set_name(msg_stanza, "message");
    xmpp_stanza_set_type(msg_stanza, txt_msg->type);
    
    xmpp_stanza_set_attribute(msg_stanza, "to", txt_msg->common_info.to);
    
    xmpp_stanza_t *msg_body = xmpp_stanza_new(ctx);
    xmpp_stanza_set_name(msg_body, "body");
    
    xmpp_stanza_t *text = xmpp_stanza_new(ctx);
    xmpp_stanza_set_text(text, txt_msg->txt);
    
    xmpp_stanza_add_child(msg_body, text);
    xmpp_stanza_release(text);
    xmpp_stanza_add_child(msg_stanza, msg_body);
    xmpp_stanza_release(msg_body);
    
    return msg_stanza;
}

int simple_convert(const char *from, const char *to, char *in_buf, size_t in_left,
                   char *out_buf, size_t out_left)
{
    iconv_t icd;
    char *pin = in_buf;
    char *pout = out_buf;
    size_t out_len = out_left;
    if ((iconv_t)-1 == (icd = iconv_open(to, from))) {
        return -1;
    }
    if ((size_t)-1 == iconv(icd, &pin, &in_left, &pout, &out_left)) {
        iconv_close(icd);
        return -1;
    }
    out_buf[out_len - out_left] = 0;
    iconv_close(icd);
    return (int)out_len - out_left;
}

void cmd_send_msg(xmpp_conn_t *conn, char *to, char *msg)
{
    im_text_msg_t txt_msg;
    txt_msg.common_info.to = to;
    txt_msg.type = "chat";
    
    
    size_t msg_len = strlen(msg);
    char *buff = malloc(msg_len * 2);
    
    if (simple_convert("GBK", "UTF-8", msg, msg_len, buff, msg_len * 2) < 0) {
        perror("GBK=>UTF8 error");
    }
    
    txt_msg.txt = buff;
    xmpp_stanza_t *msg_stanza = txt_msg_to_stanza(&txt_msg, conn);
    if (msg_stanza != NULL) {
        xmpp_send(conn, msg_stanza);
        xmpp_stanza_release(msg_stanza);
    }
    free(buff);
}

int handle_txt_msg(xmpp_conn_t *conn, xmpp_stanza_t *stanza, void *userdata)
{
    char *intext;
    xmpp_ctx_t *ctx = (xmpp_ctx_t*)userdata;
    xmpp_stanza_t *body_stanza;
    
    body_stanza = xmpp_stanza_get_child_by_name(stanza, "body");
    if (body_stanza) {
        intext = xmpp_stanza_get_text(body_stanza);
        
        // 转码
        size_t msg_len = strlen(intext);
        char *buff = malloc(msg_len * 2);
        if (simple_convert("UTF-8", "GBK", intext, msg_len, buff, msg_len * 2) < 0) {
            perror("UTF8=>GBK error");
        }
        
        printf("---------------------------------------------\n");
        printf("[Message from %s]: \n%s\n", xmpp_stanza_get_attribute(stanza, "from"), buff);
        printf("---------------------------------------------\n");
        
        free(buff);
        xmpp_free(ctx, intext);
    }
    
    return XMPP_HANDLER_AGAIN;
}