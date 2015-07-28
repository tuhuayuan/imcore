/**
 * @file	E:\projects\imcore\src\imcore.h
 *
 * @brief	IM核心结构, 所有字符串编码要求UTF8
 */

#ifndef __IMCORE_H__
#define __IMCORE_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef IMCORE_EXPORTS
#define IMCORE_API __declspec(dllexport)
#else
#define IMCORE_API __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @typedef	struct im_conn im_conn_t
 *
 * @brief	IM连接
 * 			所有的消息发送以及呼叫都有IM连接承载.
 */
typedef struct im_conn im_conn_t;

/**
 * @typedef	struct im_msg im_msg_t
 *
 * @brief	抽象消息接口
 */
typedef struct im_msg im_msg_t;

/**
 * @typedef	enum _im_conn_state
 *
 * @brief	定义了IM连接的状态.
 */
typedef enum _im_conn_state {
    ///< 初始化状态
    IM_STATE_INIT,
    ///< 正在连接
    IM_STATE_OPENING,
    ///< 连接已经打开
    IM_STATE_OPEN,
    ///< 连接已关闭
    IM_STATE_CLOSED,
} im_conn_state;

/**
 * @typedef	enum _im_call_state
 *
 * @brief	定义了呼叫状态
 */
typedef enum _im_call_state {
    ///< 连接中
    IM_CALL_CONNECTING,
    ///< 已连接
    TM_CALL_CONNECTED,
    ///< 已接受
    IM_CALL_ACCEPTED,
    ///< 连接断开
    IM_CALL_DISCONNNECTED
} im_call_state;

/**
 * @typedef	struct im_call_ctx im_voicecall_ctx_t
 *
 * @brief	呼叫会话
 */
typedef struct im_call_ctx im_voicecall_ctx_t;

/**
 * @fn	IMCORE_API bool im_init();
 *
 * @brief	初始化IM库,必须在调用其他IM方法前调用
 *
 * @author	Huayuan
 * @date	2015/7/27
 *
 * @return	true if it succeeds, false if it fails.
 */
IMCORE_API bool im_init();

/**
 * @fn	IMCORE_API void im_destroy();
 *
 * @brief	释放IM库所分配的资源
 *
 * @author	Huayuan
 * @date	2015/7/27
 */
IMCORE_API void im_destroy();

typedef void(*im_conn_state_cb)(im_conn_t *conn, im_conn_state state, int error, void *userdate);
typedef void(*im_conn_recive_cb)(im_conn_t *conn, im_msg_t *msg, void *userdate);
typedef void(*im_conn_send_cb)(im_msg_t *msg, int error, void *userdata);
typedef void(*im_file_download_cb)(int error, void *userdata);
typedef void(*im_call_state_cb)(im_voicecall_ctx_t *ctx, im_call_state state, void *userdata);
typedef int(*im_voicecall_read_cb)(im_voicecall_ctx_t *ctx,
                                   const char *inbuf, size_t *len, void *userdata);
typedef int(*im_voicecall_write_cb)(im_voicecall_ctx_t *ctx, char *outbuf, size_t maxlen,
                                    void *userdata);

/**
 * @fn	IMCORE_API im_conn_t *im_conn_new(const char *host, const char *username, const char *password, im_conn_state_cb statecb, im_conn_recive_cb msgcb, void *userdate);
 *
 * @brief	新建一个IM连接, 提供连接状态以及广播消息接收的回调函数
 *
 * @author	Huayuan
 * @date	2015/7/27
 *
 * @param	host				服务器地址域名或者IP地址
 * @param	username			用户名
 * @param	password			登录凭证
 * @param	statecb				状态回调
 * @param	msgcb				广播消息回调
 * @param [in]	userdate		自定义参数
 *
 * @return	null if it fails, else an im_conn_t*.
 */
IMCORE_API im_conn_t *im_conn_new(const char *host,
                                  const char *username,
                                  const char *password,
                                  im_conn_state_cb statecb,
                                  im_conn_recive_cb msgcb,
                                  void *userdate);

/**
 * @fn	IMCORE_API int im_conn_open(im_conn_t *conn);
 *
 * @brief	发起IM连接, 方法立即返回. 客户端在state, recive回调
 * 			里面处理状态改变, 以及消息.
 *
 * @author	Huayuan
 * @date	2015/7/28
 *
 * @param [in]	conn	要启动的连接
 *
 * @return	An int.
 */
IMCORE_API int im_conn_open(im_conn_t *conn);


IMCORE_API int im_conn_close(im_conn_t *conn);
IMCORE_API void im_conn_free(im_conn_t *conn);
IMCORE_API int im_msg_file_download(const char *remoteurl, const char *localurl,
                                    const char *secret, im_file_download_cb cb, void *userdata);
IMCORE_API uint64_t im_msg_get_id(im_msg_t *msg);
IMCORE_API char *im_msg_get_from(im_msg_t *msg);
IMCORE_API char *im_msg_get_to(im_msg_t *msg);
IMCORE_API char *im_msg_get_type(im_msg_t *msg);
IMCORE_API bool *im_msg_require_receipt(im_msg_t *msg);
IMCORE_API im_conn_t *im_msg_get_conn(im_msg_t *msg);
IMCORE_API im_msg_t *im_msg_clone(im_msg_t *msg);
IMCORE_API int im_msg_send(im_msg_t *msg, im_conn_send_cb cb, bool require, void *userdata);
IMCORE_API int im_msg_free(im_msg_t *msg);
IMCORE_API im_msg_t *im_msg_text_new(im_conn_t *conn, const char *to, const char *msg, size_t len);
IMCORE_API int im_msg_text_read(im_msg_t *textmsg, char *buff, size_t maxleng);
IMCORE_API im_msg_t *im_msg_image_new(im_conn_t *conn,
                                      const char *to, const char *localurl, bool original);
IMCORE_API int im_msg_image_info(im_msg_t *imgmsg,
                                 int *weight, int *height,
                                 char *thumbnailurl, size_t thumbnailurl_len,
                                 char *normalurl, size_t normalurl_len,
                                 char *secret);
IMCORE_API im_msg_t *im_msg_voice_new(im_conn_t *conn, const char *to, const char *localurl, long millisecond);
IMCORE_API int im_msg_voice_info(im_msg_t *voicemsg, long *millisecond,
                                 char *remoteurl, size_t *urllen,
                                 char *secret);

IMCORE_API im_msg_t *im_msg_voicecall_new(const char *to, im_call_state_cb cb);
IMCORE_API int im_voicecall_setcb(im_voicecall_ctx_t *ctx,
                                  im_voicecall_read_cb readcb, im_voicecall_write_cb writecb,
                                  void *userdata);
IMCORE_API int im_voicecall_hangup(im_voicecall_ctx_t *ctx);
IMCORE_API int im_voicecall_reject(im_voicecall_ctx_t *ctx, const char *reason);
IMCORE_API int im_voicecall_accept(im_msg_t *im_msg);

#ifdef __cplusplus
}
#endif

#endif // __IMCORE_H__