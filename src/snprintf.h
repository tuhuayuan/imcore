#ifndef __IMCORE_SNPRINTF_H__
#define __IMCORE_SNPRINTF_H__

#include <stdio.h>

int c99_vsnprintf(char *outBuf, size_t size, const char *format, va_list ap);
int c99_snprintf(char *outBuf, size_t size, const char *format, ...);

#endif // __IMCORE_SNPRINTF_H__