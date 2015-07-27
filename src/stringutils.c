#include "stringutils.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "mm.h"


char *im_strndup(const char *src, size_t maxlen)
{
    size_t len;
    char *dup;
    
    len = im_strnlen(src, maxlen);
    if (!len)
        return NULL;
        
    len++;
    dup = safe_mem_malloc(len, NULL);
    if (!dup)
        return NULL;
        
    memcpy(dup, src, len);
    return dup;
}

int im_vsnprintf(char *outBuf, size_t size, const char *format, va_list ap)
{
    int count = -1;
#if defined(_MSC_VER) && _MSC_VER < 1900
    if (size != 0)
        count = _vsnprintf_s(outBuf, size, _TRUNCATE, format, ap);
    if (count == -1)
        count = _vscprintf(format, ap);
#else     // C99标准函数
    count = vsnprintf(outBuf, size, format, ap);
#endif
    return count;
}

int im_snprintf(char *outBuf, size_t size, const char *format, ...)
{
    int count;
    va_list ap;
    
    va_start(ap, format);
    count = im_vsnprintf(outBuf, size, format, ap);
    va_end(ap);
    
    return count;
}
