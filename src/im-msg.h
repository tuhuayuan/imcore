/**
 * @file	src\im-msg.h
 *
 * @brief	声明抽象消息结构,以及操作方法.
 */
#ifndef __IMCORE_MSG_H__
#define __IMCORE_MSG_H__

#include "imcore.h"
#include "common.h"

/**
 * @typedef	void(*im_msg_free_imp)(im_msg_t *msg)
 *
 * @brief	消息释放虚函数
 */
typedef void(*im_msg_free_imp)(im_msg_t *msg);

/**
 * @typedef	void(*im_msg_send_imp)(im_msg_t *msg, im_conn_send_cb cb, bool receipt, void *userdata)
 *
 * @brief	消息发送虚函数
 */
typedef void(*im_msg_send_imp)(im_msg_t *msg, im_conn_send_cb cb, bool receipt, void *userdata);

/**
 * @struct	im_msg
 *
 * @brief	抽象消息结构体，提供了每个具体类型消息的共享字段.
 *
 * @author	Huayuan
 * @date	2015/7/27
 */
struct im_msg {
    /** @brief	64位整形唯一id. */
    uint64_t id;
    /** @brief	消息来源. */
    char *from;
    /** @brief	消息目的地. */
    char *to;
    /** @brief	消息类型字符串例如 "text" "image" "voice" "blob". */
    char *type;
    /** @brief	消息创建的时间, 并非服务器时间. */
    time_t createtime;
    /** @brief	消息对于的连接. */
    im_conn_t *conn;
    /** @brief	消息引用计数. */
    int ref;
    /** @brief	free虚函数指针 */
    im_msg_free_imp free_imp;
    /** @brief	send虚函数指针 */
    im_msg_send_imp send_imp;
};

/**
 * @fn	im_msg_t *im_msg_new(im_conn_t *conn, const char *from, const char *to, const char *type, im_msg_free_imp free_imp, im_msg_send_imp send_imp);
 *
 * @brief	新建一个消息对象, 改消息对象引用计数为1.
 *
 * @author	Huayuan
 * @date	2015/7/27
 *
 * @param [in]	conn	消息所属的IM连接.
 * @param	from			消息来源.
 * @param	to				消息目的地.
 * @param	type			消息类型.
 * @param	free_imp		释放实现函数指针
 * @param	send_imp		发送函数实现指针
 *
 * @return	返回创建的指针, 或者 NULL.
 */
im_msg_t *im_msg_new(im_conn_t *conn,
                     const char *from, const char *to, const char *type,
                     im_msg_free_imp free_imp,
                     im_msg_send_imp send_imp);

/**
 * @fn	void im_msg_init(im_msg_t *msg, im_conn_t *conn, const char *from, const char *to, const char *type, im_msg_free_imp free_imp, im_msg_send_imp send_imp);
 *
 * @brief	初始化消息对象, 消息对象引用计数设置1.
 *
 * @author	Huayuan
 * @date	2015/7/27
 *
 * @param [in]	msg 	消息对象指针.
 * @param [in]	conn	消息所属的IM连接.
 * @param	from			消息来源.
 * @param	to				消息目的地.
 * @param	type			消息类型.
 * @param	free_imp		释放实现函数指针
 * @param	send_imp		发送函数实现指针
 */

void im_msg_init(im_msg_t *msg, im_conn_t *conn,
                 const char *from, const char *to, const char *type,
                 im_msg_free_imp free_imp,
                 im_msg_send_imp send_imp);

/**
 * @fn	void im_msg_destroy(im_msg_t *msg);
 *
 * @brief	反初始化消息对象, 引用计数-1.(注意:这里不释放)
 *
 * @author	Huayuan
 * @date	2015/7/27
 *
 * @param   msg 	消息对象指针
 */
void im_msg_destroy(im_msg_t *msg);


#endif // __IMCORE_MSG_H__