#ifndef __IMCORE_COMMON_H__
#define __IMCORE_COMMON_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

// string处理
#if defined(_MSC_VER) && _MSC_VER < 1900
#include "snprintf.h"

#define imcore_snprintf c99_snprintf
#define imcore_vsnprintf c99_vsnprintf
#else
#define imcore_snprintf snprintf
#define imcore_vsnprintf vsnprintf
#endif

// 该死的vs不认识inline
#if defined(_WIN32) && !defined(__cplusplus)
#define inline __inline
#endif

// 大小写不敏感比较
#if defined(WIN32) || defined(WIN64)
#define strcasecmp _stricmp
#endif

#endif //__IMCORE_COMMON_H__