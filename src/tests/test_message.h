/* test_message.h
*  ʵ�ֲ��Ҳ�����Ϣ�ı���Ϣ�շ�
*/
#ifndef IMCORE_TEST_MESSAGE_H
#define IMCORE_TEST_MESSAGE_H

#include "xmpp.h"
#include "imcore-inl.h"

// �����ı���Ϣ
void cmd_send_msg(xmpp_conn_t *conn, char *to, char *msg);

// �����ı���Ϣ
int handle_txt_msg(xmpp_conn_t *conn, xmpp_stanza_t * stanza, void *userdata);

#endif // IMCORE_TEST_MESSAGE_H