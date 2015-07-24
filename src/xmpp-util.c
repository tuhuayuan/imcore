/* util.c
 * xmpp-inl.h ���涨���һЩ���ߺ���
 */
#include "xmpp-inl.h"

char *xmpp_strdup(const xmpp_ctx_t *ctx, const char *s)
{
    size_t len;
    char *copy;

    len = strlen(s);
    copy = xmpp_alloc((void*)ctx, len + 1);

    if (!copy) {
        xmpp_error(ctx, "xmpp", "failed to allocate required memory");
        return NULL;
    }

    memcpy(copy, s, len + 1);
    return copy;
}

void util_hash_free(const xmpp_ctx_t* const ctx, void* p)
{
    xmpp_free(ctx, p);
}
