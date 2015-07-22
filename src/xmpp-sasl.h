/* sasl.h
 * sasl认证支持 plain、digest md5
 */

#ifndef __IMCORE_XMPP_SASL_H__
#define __IMCORE_XMPP_SASL_H__

#include "xmpp-inl.h"

char *sasl_plain(xmpp_ctx_t *ctx,
                 const char *authid,
                 const char *password);
char *sasl_digest_md5(xmpp_ctx_t *ctx,
                      const char *challenge,
                      const char *jid,
                      const char *password);

// Base64
int base64_encoded_len(xmpp_ctx_t *ctx,
                       const unsigned len);
char *base64_encode(xmpp_ctx_t *ctx,
                    const unsigned char *const buffer,
                    const unsigned len);
int base64_decoded_len(xmpp_ctx_t *ctx,
                       const char *const buffer,
                       const unsigned len);
unsigned char *base64_decode(xmpp_ctx_t *ctx,
                             const char *const buffer,
                             const unsigned len);

#endif /* __IMCORE_XMPP_SASL_H__ */
