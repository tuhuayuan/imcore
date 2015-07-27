/* md5.h
 * md5实现
 */

#ifndef _IMCORE_XMPP_MD5_H
#define _IMCORE_XMPP_MD5_H

#include <stdint.h>

struct MD5Context {
    uint32_t buf[4];
    uint32_t bits[2];
    unsigned char in[64];
};

void MD5Init(struct MD5Context *context);
void MD5Update(struct MD5Context *context, unsigned char const *buf,
               uint32_t len);
void MD5Final(unsigned char digest[16], struct MD5Context *context);

#ifdef DEBUG_MD5
void MD5DumpBytes(unsigned char *b, int len);
#endif

#endif // _IMCORE_XMPP_MD5_H
