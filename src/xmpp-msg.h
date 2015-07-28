/**
 * @file	src\xmpp-msg.h
 *
 * @brief	RFC6121 5. 交换消息
 * 			XEP-0184 消息回执
 */
#include "xmpp.h"

xmpp_stanza_t *xmpp_msg_create(const char *id,
                               const char *to,
                               const char *from,
                               const char *type,
                               const char *body);
bool xmpp_msg_valid(xmpp_stanza_t *raw);
void xmpp_msg_extend(xmpp_stanza_t *msg_stanza, xmpp_stanza_t *child);
char *xmpp_msg_get_id(xmpp_stanza_t *msg_stanza);
char *xmpp_msg_get_to(xmpp_stanza_t *msg_stanza);
char *xmpp_msg_get_from(xmpp_stanza_t *msg_stanza);
char *xmpp_msg_get_type(xmpp_stanza_t *msg_stanza);
int xmpp_msg_get_body(xmpp_stanza_t *msg_stanza, char *buff, size_t maxlen);

typedef enum {
    RECEIPT_REQUEST,
    RECEIPT_RECEIVED
} XMPP_RECEIPT_TYPE;

xmpp_stanza_t *xmpp_msg_receipt_create(const char *id, int type);
xmpp_stanza_t *xmpp_msg_receipt_find(xmpp_stanza_t *msg_stanza);
char *xmpp_msg_receipt_get_id(xmpp_stanza_t *receipt_stanza);
int xmpp_msg_receipt_set_id(xmpp_stanza_t *receipt_stanza);
