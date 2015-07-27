/* jid.c
 * JID 帮助函数
 */
#include "xmpp-inl.h"


char *xmpp_jid_new(xmpp_ctx_t *ctx, const char *node,
                   const char *domain,
                   const char *resource)
{
    char *result;
    int len,nlen,dlen,rlen;
    if (domain == NULL) return NULL;
    dlen = strlen(domain);
    nlen = (node) ? strlen(node) + 1 : 0;
    rlen = (resource) ? strlen(resource) + 1 : 0;
    len = nlen + dlen + rlen;
    result = xmpp_alloc(ctx, len + 1);
    if (result != NULL) {
        if (node != NULL) {
            memcpy(result, node, nlen - 1);
            result[nlen-1] = '@';
        }
        memcpy(result + nlen, domain, dlen);
        if (resource != NULL) {
            result[nlen+dlen] = '/';
            memcpy(result+nlen+dlen+1, resource, rlen - 1);
        }
        result[nlen+dlen+rlen] = '\0';
    }
    return result;
}


char *xmpp_jid_bare(xmpp_ctx_t *ctx, const char *jid)
{
    char *result;
    const char *c;
    c = strchr(jid, '/');
    if (c == NULL) return xmpp_strdup(ctx, jid);
    result = xmpp_alloc(ctx, c-jid+1);
    if (result != NULL) {
        memcpy(result, jid, c-jid);
        result[c-jid] = '\0';
    }
    return result;
}

char *xmpp_jid_node(xmpp_ctx_t *ctx, const char *jid)
{
    char *result = NULL;
    const char *c;
    c = strchr(jid, '@');
    if (c != NULL) {
        result = xmpp_alloc(ctx, (c-jid) + 1);
        if (result != NULL) {
            memcpy(result, jid, (c-jid));
            result[c-jid] = '\0';
        }
    }
    return result;
}

char *xmpp_jid_domain(xmpp_ctx_t *ctx, const char *jid)
{
    char *result = NULL;
    const char *c,*s;
    c = strchr(jid, '@');
    if (c == NULL) {
        c = jid;
    } else {
        c++;
    }
    s = strchr(c, '/');
    if (s == NULL) {
        s = c + strlen(c);
    }
    result = xmpp_alloc(ctx, (s-c) + 1);
    if (result != NULL) {
        memcpy(result, c, (s-c));
        result[s-c] = '\0';
    }
    return result;
}

char *xmpp_jid_resource(xmpp_ctx_t *ctx, const char *jid)
{
    char *result = NULL;
    const char *c;
    int len;
    c = strchr(jid, '/');
    if (c != NULL) {
        c++;
        len = strlen(c);
        result = xmpp_alloc(ctx, len + 1);
        if (result != NULL) {
            memcpy(result, c, len);
            result[len] = '\0';
        }
    }
    return result;
}
