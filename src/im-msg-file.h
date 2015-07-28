/**
 * @file	src\im-msg-file.h
 *
 * @brief	文件类消息通用操作
 */
#ifndef _IM_MEDIA_TRANSFER_H
#define _IM_MEDIA_TRANSFER_H

#include "im-thread.h"
#include "xmpp.h"

#define IM_MSG_FILE_ID "im_msg_file_id_oob"

int msg_file_handler(xmpp_conn_t *conn, xmpp_stanza_t *stanza, void *userdata);

#endif // _IM_MEDIA_TRANSFER_H