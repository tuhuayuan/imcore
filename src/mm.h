/*
* mm.h
* 内存管理模块，提供动态内存分配管理以及内存溢出检查(DEBUG)
*/
#ifndef _IMCORE_MM_H
#define _IMCORE_MM_H

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#if defined(WIN32) || defined(WIN64)
#define __FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
#else
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

typedef void (*safe_mem_check_cb)(void *p, size_t s, const char *file, uint64_t line,
                                  void *mem_userdata, void *cb_userdata);
void ring_init_check();
void *ring_malloc(size_t s, char *file, uint64_t line, void *userdata);
void *ring_calloc(size_t s, char *file, uint64_t line, void *userdata);
void ring_free(void *p);
void *ring_realloc(void *p, size_t size, char *file, uint64_t line, void *userdata);
void ring_clean_check(safe_mem_check_cb cb, void *cb_userdata);

#ifdef _DEBUG
#define safe_mem_init ring_init_check
#define safe_mem_malloc(s, d) ring_malloc(s, __FILENAME__, __LINE__, d)
#define safe_mem_calloc(s, d) ring_calloc(s, __FILENAME__, __LINE__, d)
#define safe_mem_realloc(p, s, d) ring_realloc(p, s, __FILENAME__, __LINE__, d)
#define safe_mem_free(p) ring_free(p)
#define safe_mem_check(cb, data) ring_clean_check(cb, data)
#else
#define safe_mem_init
#define safe_mem_malloc(s, d) malloc(s)
#define safe_mem_calloc(s, d) calloc(s)
#define safe_mem_realloc(p, s, d) realloc(p, s)
#define safe_mem_free(p) free(p)
#define safe_mem_check
#endif

#endif // _IMCORE_MM_H