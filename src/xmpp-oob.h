/**
 * @file	src\xmpp-oob.h
 *
 * @brief	实现XEP-0066: Out of Band Data
 * 			加上针对图片以及声音文件的扩展信息
 */
#ifndef __XMPP_OOB_H__
#define __XMPP_OOB_H__

#include "xmpp.h"

xmpp_stanza_t *xmpp_oob_create(const char *id, const char *url, const char *desc);
bool xmpp_obb_valid(xmpp_stanza_t *raw);
xmpp_stanza_t *xmpp_oob_accept(xmpp_stanza_t *oob_stanza);
xmpp_stanza_t *xmpp_oob_reject(xmpp_stanza_t *oob_stanza, int code);
const char *xmpp_oob_get_value(xmpp_stanza_t *oob_stanza, const char *key);
int xmpp_oob_set_value(xmpp_stanza_t *oob_stanza, const char *key, const char *value);


#endif // __XMPP_OOB_H__