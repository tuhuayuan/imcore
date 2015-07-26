/* sasl.c
 * 实现 SASL认证
 */

#include "md5.h"
#include "base64.h"

#include "xmpp-inl.h"

// PLAIN 认证
char *sasl_plain(xmpp_ctx_t *ctx, const char *authid, const char *password)
{
    int idlen, passlen;
    char *result = NULL;
    char *msg;
    idlen = strlen(authid);
    passlen = strlen(password);
    msg = xmpp_alloc(ctx, 2 + idlen + passlen);
    if (msg != NULL) {
        msg[0] = '\0';
        memcpy(msg+1, authid, idlen);
        msg[1+idlen] = '\0';
        memcpy(msg+1+idlen+1, password, passlen);
        result = base64_encode((unsigned char *)msg, 2 + idlen + passlen);
        xmpp_free(ctx, msg);
    }
    return result;
}

static char *_make_string(xmpp_ctx_t *ctx, const char *s, const unsigned len)
{
    char *result;
    result = xmpp_alloc(ctx, len + 1);
    if (result != NULL) {
        memcpy(result, s, len);
        result[len] = '\0';
    }
    return result;
}

static char *_make_quoted(xmpp_ctx_t *ctx, const char *s)
{
    char *result;
    int len = strlen(s);
    result = xmpp_alloc(ctx, len + 3);
    if (result != NULL) {
        result[0] = '"';
        memcpy(result+1, s, len);
        result[len+1] = '"';
        result[len+2] = '\0';
    }
    return result;
}

// 解析服务器md5挑战
static hash_t *_parse_digest_challenge(xmpp_ctx_t *ctx, const char *msg)
{
    hash_t *result;
    unsigned char *text;
    char *key, *value;
    unsigned char *s, *t;
    text = base64_decode(msg, strlen(msg));
    if (text == NULL) {
        xmpp_error(ctx, "SASL", "couldn't Base64 decode challenge!");
        return NULL;
    }
    result = hash_new(10, xmpp_hash_free);
    if (result != NULL) {
        s = text;
        while (*s != '\0') {
            /* skip any leading commas and spaces */
            while ((*s == ',') || (*s == ' ')) s++;
            /* accumulate a key ending at '=' */
            t = s;
            while ((*t != '=') && (*t != '\0')) t++;
            if (*t == '\0') break; /* bad string */
            key = _make_string(ctx, (char *)s, (t-s));
            if (key == NULL) break;
            /* advance our start pointer past the key */
            s = t + 1;
            t = s;
            /* if we see quotes, grab the string in between */
            if ((*s == '\'') || (*s == '"')) {
                t++;
                while ((*t != *s) && (*t != '\0'))
                    t++;
                value = _make_string(ctx, (char *)s+1, (t-s-1));
                if (*t == *s) {
                    s = t + 1;
                } else {
                    s = t;
                }
                /* otherwise, accumulate a value ending in ',' or '\0' */
            } else {
                while ((*t != ',') && (*t != '\0')) t++;
                value = _make_string(ctx, (char *)s, (t-s));
                s = t;
            }
            if (value == NULL) {
                xmpp_free(ctx, key);
                break;
            }
            /* TODO: check for collisions per spec */
            hash_add(result, key, value);
            /* hash table now owns the value, free the key */
            xmpp_free(ctx, key);
        }
    }
    xmpp_free(ctx, text);
    return result;
}

static void _digest_to_hex(const char *digest, char *hex)
{
    int i;
    const char hexdigit[] = "0123456789abcdef";
    for (i = 0; i < 16; i++) {
        *hex++ = hexdigit[ (digest[i] >> 4) & 0x0F ];
        *hex++ = hexdigit[ digest[i] & 0x0F ];
    }
}

static char *_add_key(xmpp_ctx_t *ctx, hash_t *table, const char *key,
                      char *buf, int *len, int quote)
{
    int olen,nlen;
    int keylen, valuelen;
    const char *value, *qvalue;
    char *c;
    /* allocate a zero-length string if necessary */
    if (buf == NULL) {
        buf = xmpp_alloc(ctx, 1);
        buf[0] = '\0';
    }
    if (buf == NULL) return NULL;
    /* get current string length */
    olen = strlen(buf);
    value = hash_get(table, key);
    if (value == NULL) {
        xmpp_error(ctx, "SASL", "couldn't retrieve value for '%s'", key);
        value = "";
    }
    if (quote) {
        qvalue = _make_quoted(ctx, value);
    } else {
        qvalue = value;
    }
    /* added length is key + '=' + value */
    /*   (+ ',' if we're not the first entry   */
    keylen = strlen(key);
    valuelen = strlen(qvalue);
    nlen = (olen ? 1 : 0) + keylen + 1 + valuelen + 1;
    buf = xmpp_realloc(ctx, buf, olen+nlen);
    if (buf != NULL) {
        c = buf + olen;
        if (olen) *c++ = ',';
        memcpy(c, key, keylen);
        c += keylen;
        *c++ = '=';
        memcpy(c, qvalue, valuelen);
        c += valuelen;
        *c++ = '\0';
    }
    if (quote)
        xmpp_free(ctx, (char *)qvalue);
        
    return buf;
}

// SASL DIGEST-MD5 认证实现, 具体参见维基百科
char *sasl_digest_md5(xmpp_ctx_t *ctx, const char *challenge,
                      const char *jid, const char *password)
{
    hash_t *table;
    char *result = NULL;
    char *node, *domain, *realm;
    char *value;
    char *response;
    int rlen;
    struct MD5Context MD5;
    unsigned char digest[16], HA1[16], HA2[16];
    char hex[32];
    /* our digest response is
    Hex( KD( HEX(MD5(A1)),
    nonce ':' nc ':' cnonce ':' qop ':' HEX(MD5(A2))
    ))
    
       where KD(k, s) = MD5(k ':' s),
    A1 = MD5( node ':' realm ':' password ) ':' nonce ':' cnonce
    A2 = "AUTHENTICATE" ':' "xmpp/" domain
    
       If there is an authzid it is ':'-appended to A1 */
    /* parse the challenge */
    table = _parse_digest_challenge(ctx, challenge);
    if (table == NULL) {
        xmpp_error(ctx, "SASL", "couldn't parse digest challenge");
        return NULL;
    }
    node = xmpp_jid_node(ctx, jid);
    domain = xmpp_jid_domain(ctx, jid);
    /* generate default realm of domain if one didn't come from the
       server */
    realm = hash_get(table, "realm");
    if (realm == NULL || strlen(realm) == 0) {
        hash_add(table, "realm", xmpp_strdup(ctx, domain));
        realm = hash_get(table, "realm");
    }
    /* add our response fields */
    hash_add(table, "username", xmpp_strdup(ctx, node));
    /* TODO: generate a random cnonce */
    hash_add(table, "cnonce", xmpp_strdup(ctx, "00DEADBEEF00"));
    hash_add(table, "nc", xmpp_strdup(ctx, "00000001"));
    hash_add(table, "qop", xmpp_strdup(ctx, "auth"));
    value = xmpp_alloc(ctx, 5 + strlen(domain) + 1);
    memcpy(value, "xmpp/", 5);
    memcpy(value+5, domain, strlen(domain));
    value[5+strlen(domain)] = '\0';
    hash_add(table, "digest-uri", value);
    /* generate response */
    /* construct MD5(node : realm : password) */
    MD5Init(&MD5);
    MD5Update(&MD5, (unsigned char *)node, strlen(node));
    MD5Update(&MD5, (unsigned char *)":", 1);
    MD5Update(&MD5, (unsigned char *)realm, strlen(realm));
    MD5Update(&MD5, (unsigned char *)":", 1);
    MD5Update(&MD5, (unsigned char *)password, strlen(password));
    MD5Final(digest, &MD5);
    /* digest now contains the first field of A1 */
    MD5Init(&MD5);
    MD5Update(&MD5, digest, 16);
    MD5Update(&MD5, (unsigned char *)":", 1);
    value = hash_get(table, "nonce");
    MD5Update(&MD5, (unsigned char *)value, strlen(value));
    MD5Update(&MD5, (unsigned char *)":", 1);
    value = hash_get(table, "cnonce");
    MD5Update(&MD5, (unsigned char *)value, strlen(value));
    MD5Final(digest, &MD5);
    /* now digest is MD5(A1) */
    memcpy(HA1, digest, 16);
    /* construct MD5(A2) */
    MD5Init(&MD5);
    MD5Update(&MD5, (unsigned char *)"AUTHENTICATE:", 13);
    value = hash_get(table, "digest-uri");
    MD5Update(&MD5, (unsigned char *)value, strlen(value));
    if (strcmp(hash_get(table, "qop"), "auth") != 0) {
        MD5Update(&MD5, (unsigned char *)":00000000000000000000000000000000",
                  33);
    }
    MD5Final(digest, &MD5);
    memcpy(HA2, digest, 16);
    /* construct response */
    MD5Init(&MD5);
    _digest_to_hex((char *)HA1, hex);
    MD5Update(&MD5, (unsigned char *)hex, 32);
    MD5Update(&MD5, (unsigned char *)":", 1);
    value = hash_get(table, "nonce");
    MD5Update(&MD5, (unsigned char *)value, strlen(value));
    MD5Update(&MD5, (unsigned char *)":", 1);
    value = hash_get(table, "nc");
    MD5Update(&MD5, (unsigned char *)value, strlen(value));
    MD5Update(&MD5, (unsigned char *)":", 1);
    value = hash_get(table, "cnonce");
    MD5Update(&MD5, (unsigned char *)value, strlen(value));
    MD5Update(&MD5, (unsigned char *)":", 1);
    value = hash_get(table, "qop");
    MD5Update(&MD5, (unsigned char *)value, strlen(value));
    MD5Update(&MD5, (unsigned char *)":", 1);
    _digest_to_hex((char *)HA2, hex);
    MD5Update(&MD5, (unsigned char *)hex, 32);
    MD5Final(digest, &MD5);
    response = xmpp_alloc(ctx, 32+1);
    _digest_to_hex((char *)digest, hex);
    memcpy(response, hex, 32);
    response[32] = '\0';
    hash_add(table, "response", response);
    /* construct reply */
    result = NULL;
    rlen = 0;
    result = _add_key(ctx, table, "username", result, &rlen, 1);
    result = _add_key(ctx, table, "realm", result, &rlen, 1);
    result = _add_key(ctx, table, "nonce", result, &rlen, 1);
    result = _add_key(ctx, table, "cnonce", result, &rlen, 1);
    result = _add_key(ctx, table, "nc", result, &rlen, 0);
    result = _add_key(ctx, table, "qop", result, &rlen, 0);
    result = _add_key(ctx, table, "digest-uri", result, &rlen, 1);
    result = _add_key(ctx, table, "response", result, &rlen, 0);
    result = _add_key(ctx, table, "charset", result, &rlen, 0);
    xmpp_free(ctx, node);
    xmpp_free(ctx, domain);
    hash_release(table); /* also frees value strings */
    /* reuse response for the base64 encode of our result */
    response = base64_encode((unsigned char *)result, strlen(result));
    xmpp_free(ctx, result);
    return response;
}

