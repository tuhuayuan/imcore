/**
 * @file	src\im-msg.h
 *
 * @brief	����������Ϣ�ṹ,�Լ���������.
 */
#ifndef __IMCORE_MSG_H__
#define __IMCORE_MSG_H__

#include "imcore.h"
#include "common.h"

/**
 * @typedef	void(*im_msg_free_imp)(im_msg_t *msg)
 *
 * @brief	��Ϣ�ͷ��麯��
 */
typedef void(*im_msg_free_imp)(im_msg_t *msg);

/**
 * @typedef	void(*im_msg_send_imp)(im_msg_t *msg, im_conn_send_cb cb, bool receipt, void *userdata)
 *
 * @brief	��Ϣ�����麯��
 */
typedef void(*im_msg_send_imp)(im_msg_t *msg, im_conn_send_cb cb, bool receipt, void *userdata);

/**
 * @struct	im_msg
 *
 * @brief	������Ϣ�ṹ�壬�ṩ��ÿ������������Ϣ�Ĺ����ֶ�.
 *
 * @author	Huayuan
 * @date	2015/7/27
 */
struct im_msg {
    /** @brief	64λ����Ψһid. */
    uint64_t id;
    /** @brief	��Ϣ��Դ. */
    char *from;
    /** @brief	��ϢĿ�ĵ�. */
    char *to;
    /** @brief	��Ϣ�����ַ������� "text" "image" "voice" "blob". */
    char *type;
    /** @brief	��Ϣ������ʱ��, ���Ƿ�����ʱ��. */
    time_t createtime;
    /** @brief	��Ϣ���ڵ�����. */
    im_conn_t *conn;
    /** @brief	��Ϣ���ü���. */
    int ref;
    /** @brief	free�麯��ָ�� */
    im_msg_free_imp free_imp;
    /** @brief	send�麯��ָ�� */
    im_msg_send_imp send_imp;
};

/**
 * @fn	im_msg_t *im_msg_new(im_conn_t *conn, const char *from, const char *to, const char *type, im_msg_free_imp free_imp, im_msg_send_imp send_imp);
 *
 * @brief	�½�һ����Ϣ����, ����Ϣ�������ü���Ϊ1.
 *
 * @author	Huayuan
 * @date	2015/7/27
 *
 * @param [in]	conn	��Ϣ������IM����.
 * @param	from			��Ϣ��Դ.
 * @param	to				��ϢĿ�ĵ�.
 * @param	type			��Ϣ����.
 * @param	free_imp		�ͷ�ʵ�ֺ���ָ��
 * @param	send_imp		���ͺ���ʵ��ָ��
 *
 * @return	���ش�����ָ��, ���� NULL.
 */
im_msg_t *im_msg_new(im_conn_t *conn,
                     const char *from, const char *to, const char *type,
                     im_msg_free_imp free_imp,
                     im_msg_send_imp send_imp);

/**
 * @fn	void im_msg_init(im_msg_t *msg, im_conn_t *conn, const char *from, const char *to, const char *type, im_msg_free_imp free_imp, im_msg_send_imp send_imp);
 *
 * @brief	��ʼ����Ϣ����, ��Ϣ�������ü�������1.
 *
 * @author	Huayuan
 * @date	2015/7/27
 *
 * @param [in]	msg 	��Ϣ����ָ��.
 * @param [in]	conn	��Ϣ������IM����.
 * @param	from			��Ϣ��Դ.
 * @param	to				��ϢĿ�ĵ�.
 * @param	type			��Ϣ����.
 * @param	free_imp		�ͷ�ʵ�ֺ���ָ��
 * @param	send_imp		���ͺ���ʵ��ָ��
 */

void im_msg_init(im_msg_t *msg, im_conn_t *conn,
                 const char *from, const char *to, const char *type,
                 im_msg_free_imp free_imp,
                 im_msg_send_imp send_imp);

/**
 * @fn	void im_msg_destroy(im_msg_t *msg);
 *
 * @brief	����ʼ����Ϣ����, ���ü���-1.(ע��:���ﲻ�ͷ�)
 *
 * @author	Huayuan
 * @date	2015/7/27
 *
 * @param   msg 	��Ϣ����ָ��
 */
void im_msg_destroy(im_msg_t *msg);


#endif // __IMCORE_MSG_H__