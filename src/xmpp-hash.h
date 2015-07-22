/************************************************************************/
/* hash.h 哈希表                                                         */
/************************************************************************/
#ifndef __IMCORE_XMPP_HASH_H__
#define __IMCORE_XMPP_HASH_H__

#include "xmpp-inl.h"

// 私有结构定义
typedef struct _hash_t hash_t;

// 自定义释放回调函数签名
typedef void (*hash_free_func)(const xmpp_ctx_t* const ctx, void* p);

// 创建、克隆、释放
hash_t *hash_new(xmpp_ctx_t* const ctx, const int size, hash_free_func free);
hash_t *hash_clone(hash_t * const table);
void hash_release(hash_t * const table);

// hash表操作
int hash_add(hash_t *table, const char * const key, void *data);
void *hash_get(hash_t *table, const char *key);
int hash_drop(hash_t *table, const char *key);
int hash_num_keys(hash_t *table);

// hash迭代器
typedef struct _hash_iterator_t hash_iterator_t;
hash_iterator_t *hash_iter_new(hash_t *table);
void hash_iter_release(hash_iterator_t *iter);
const char * hash_iter_next(hash_iterator_t *iter);

#endif // __IMCORE_XMPP_HASH_H__ 
