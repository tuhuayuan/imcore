/**
 * @file    src\im-msg-text.h
 *
 * @brief   实现文本消息发送
 */
#ifndef __IMCORE_MSG_TEXT_H__
#define __IMCORE_MSG_TEXT_H__

#include "im-msg.h"
#include "xmpp.h"

/**
 * @fn	int msg_text_handler(xmpp_conn_t *conn, xmpp_stanza_t *stanza, void *userdata);
 *
 * @brief	原始文本消息处理
 *
 * @author	Huayuan
 * @date	2015/7/28
 *
 * @param [in]	conn		xmpp信号连接
 * @param [in]	stanza  	包含文本消息的xml节
 * @param [in]	userdata	im_conn_t指针
 *
 * @return	0 or -1.
 */
int msg_text_handler(xmpp_conn_t *xmpp_conn, xmpp_stanza_t *stanza, void *userdata);


#endif // __IMCORE_MSG_TEXT_H__