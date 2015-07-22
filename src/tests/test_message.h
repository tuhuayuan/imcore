/* test_message.h
*  实现并且测试消息文本消息收发
*/
#ifndef IMCORE_TEST_MESSAGE_H
#define IMCORE_TEST_MESSAGE_H

#include "xmpp.h"
#include "imcore-inl.h"

// 发送文本消息
void cmd_send_msg(xmpp_conn_t *conn, char *to, char *msg);

// 处理文本消息
int handle_txt_msg(xmpp_conn_t *conn, xmpp_stanza_t * stanza, void *userdata);

#endif // IMCORE_TEST_MESSAGE_H