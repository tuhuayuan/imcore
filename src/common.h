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
#define imcore_snprintf evutil_snprintf
#define imcore_vsnprintf evutil_vsnprintf

// 该死的vs不认识inline
#if defined(_WIN32) && !defined(__cplusplus)
#define inline __inline
#endif

// 大小写不敏感比较
#if defined(WIN32) || defined(WIN64)
#define strcasecmp _stricmp
#endif

#endif //__IMCORE_COMMON_H__