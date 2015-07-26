/*
* file-transfer.h
* 文件传输管理模块
* XEP-0096: SI.
*   XEP-0096: SI File Transfer.
*   XEP-0066: Out of Band Data use http.
*   XEP-0047: In-Band Bytestreams.
*/
#ifndef _IM_MEDIA_TRANSFER_H
#define _IM_MEDIA_TRANSFER_H

#include "imcore-thread.h"
#include "xmpp.h"

/**
 * @brief [brief description]
 * @details [long description]
 * 
 */
typedef struct file_transfer file_transfer_t;

/**
 * @brief 
 * @details [long description]
 * 
 * @param conn [description]
 * @param work_thread [description]
 * 
 * @return [description]
 */
file_transfer_t *file_transfer_create(xmpp_conn_t *conn, im_thread_t *work_thread);
void file_transfer_destroy(file_transfer_t *transfer);

typedef struct 
typedef int(*file_transfer_open_cb)(file_transfer_t *transfer, int result, void *userdata);
int file_transfer_open(file_transfer_t *transfer, const char *to, void *userdata);
void file_transfer_close(file_transfer_t *transfer);


#endif // _IM_MEDIA_TRANSFER_H