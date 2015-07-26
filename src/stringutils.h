/*
* stringutils.h �ַ�������
*/
#ifndef __IMCORE_STRINGUTILS_H
#define __IMCORE_STRINGUTILS_H

#include <string.h>
#include <stdarg.h>

// �ַ����Ƚ�
#define im_strcmp strcmp
#define im_strncmp strncmp
#if defined(_MSC_VER)
#define im_stricmp _strcmpi
#define im_strincmp _strnicmp
#else
#define im_stricmp strcasecmp
#define im_strnicmp strncasecmp
#endif

// ���ȼ���
#define im_strnlen strnlen
#define im_strlen strlen

// �ַ�������
char *im_strndup(const char *src, size_t maxlen);

// ��ʽ�ַ�����ӡ
int im_vsnprintf(char *dest, size_t size, const char *format, va_list ap);
int im_snprintf(char *dest, size_t size, const char *format, ...);

#endif //__IMCORE_STRINGUTILS_H