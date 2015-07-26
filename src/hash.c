/*
 * hash.c hash表实现
*/
#include "hash.h"
#include "mm.h"
#include "stringutils.h"

typedef struct _hashentry_t hashentry_t;
struct _hashentry_t {
    hashentry_t *next;
    char *key;
    void *value;
};

struct _hash_t {
    unsigned int ref;
    hash_free_func free;
    int length;                         // hash槽的数量，越多性能越高，如果为1则退化成为链表
    int num_keys;
    hashentry_t **entries;
};

struct _hash_iterator_t {
    unsigned int ref;
    hash_t *table;
    hashentry_t *entry;
    int index;
};

// 如果不提供free函数，插入时如果key冲突将导致插入失败
hash_t *hash_new(int size, hash_free_func free)
{
    hash_t *result = NULL;
    result = safe_mem_malloc(sizeof(hash_t), NULL);
    if (result != NULL) {
        result->entries = safe_mem_calloc(size*sizeof(hashentry_t*), NULL);
        if (result->entries == NULL) {
            safe_mem_free(result);
            return NULL;
        }
        
        result->length = size;
        result->free = free;
        result->num_keys = 0;
        result->ref = 1;
    }
    return result;
}

hash_t *hash_clone(hash_t *table)
{
    table->ref++;
    return table;
}

void hash_release(hash_t *table)
{
    hashentry_t *entry, *next;
    int i;
    if (table->ref > 1)
        table->ref--;
    else {
        for (i = 0; i < table->length; i++) {
            entry = table->entries[i];
            while (entry != NULL) {
                next = entry->next;
                safe_mem_free(entry->key);
                if (table->free)
                    table->free(entry->value);
                safe_mem_free(entry);
                entry = next;
            }
        }
        safe_mem_free(table->entries);
        safe_mem_free(table);
    }
}

static int _hash_key(hash_t *table, const char *key)
{
    int hash = 0;
    int shift = 0;
    const char *c = key;
    while (*c != '\0') {
        hash ^= ((int)*c++ << shift);
        shift += 8;
        if (shift > 24) shift = 0;
    }
    return hash % table->length;
}

int hash_add(hash_t *table, const char *key, void *data)
{
    hashentry_t *entry = NULL;
    int index = _hash_key(table, key);
    if (table->free) {
        hash_drop(table, key);
    } else {
        // 没有指定释放函数，有冲突的话插入失败
        if (!hash_get(table, key)) {
            return -1;
        }
    }
    
    entry = safe_mem_calloc(sizeof(hashentry_t), NULL);
    if (!entry)
        return -1;
        
    entry->key = im_strndup(key, im_strlen(key));
    if (!entry->key) {
        safe_mem_free(entry);
        return -1;
    }
    entry->value = data;
    
    entry->next = table->entries[index];
    table->entries[index] = entry;
    table->num_keys++;
    return 0;
}

// 获取一个键值
void *hash_get(hash_t *table, const char *key)
{
    hashentry_t *entry;
    int index = _hash_key(table, key);
    void *result = NULL;
    
    entry = table->entries[index];
    while (entry != NULL) {
        if (!im_strcmp(key, entry->key)) {
            // 匹配
            result = entry->value;
            return result;
        }
        entry = entry->next;
    }
    // 没有匹配
    return result;
}

int hash_drop(hash_t *table, const char *key)
{
    hashentry_t *entry, *prev;
    int index = _hash_key(table, key);
    
    // entry和key都需要释放
    entry = table->entries[index];
    prev = NULL;
    
    while (entry != NULL) {
        if (!im_strcmp(key, entry->key)) {
            safe_mem_free(entry->key);
            // 自定义释放
            if (table->free)
                table->free(entry->value);
                
            if (prev == NULL) {
                table->entries[index] = entry->next;
            } else {
                prev->next = entry->next;
            }
            
            safe_mem_free(entry);
            table->num_keys--;
            return 0;
        }
        prev = entry;
        entry = entry->next;
    }
    // key不存在
    return -1;
}

int hash_num_keys(hash_t *table)
{
    return table->num_keys;
}

hash_iterator_t *hash_iter_new(hash_t *table)
{
    hash_iterator_t *iter;
    iter = safe_mem_malloc(sizeof(*iter), NULL);
    if (iter != NULL) {
        iter->ref = 1;
        iter->table = hash_clone(table);          // 增加引用计数
        iter->entry = NULL;
        iter->index = -1;
    }
    return iter;
}

void hash_iter_release(hash_iterator_t *iter)
{
    iter->ref--;
    if (iter->ref <= 0) {
        hash_release(iter->table);
        safe_mem_free(iter);
    }
}

const char *hash_iter_next(hash_iterator_t *iter)
{
    hash_t *table = iter->table;
    hashentry_t *entry = iter->entry;
    int i;
    
    // 迭代出来的keyvalue数据的顺序是不确定的
    if (entry != NULL)
        entry = entry->next;
        
    if (entry == NULL) {
        i = iter->index + 1;
        while (i < iter->table->length) {
            entry = table->entries[i];
            if (entry != NULL) {
                iter->index = i;
                break;
            }
            i++;
        }
    }
    if (entry == NULL) {
        return NULL;
    }
    
    iter->entry = entry;
    return entry->key;
}

