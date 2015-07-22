#ifndef __IMCORE_COMMON_H__
#define __IMCORE_COMMON_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

// string����
#define imcore_snprintf evutil_snprintf
#define imcore_vsnprintf evutil_vsnprintf

// ������vs����ʶinline
#if defined(_WIN32) && !defined(__cplusplus)
#define inline __inline
#endif

// ��Сд�����бȽ�
#if defined(WIN32) || defined(WIN64)
#define strcasecmp _stricmp
#endif

#endif //__IMCORE_COMMON_H__