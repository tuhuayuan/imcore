#include "mm.h"

#include <stdio.h>
#include "list.h"

typedef struct ring_mem {
    void *p;
    size_t s;
    char *file;
    uint64_t line;
    void *userdata;

    // Ë«Á´±í
    struct list_head ring;
} ring_mem_t;

static struct list_head ring_head;

void ring_init_check()
{
    INIT_LIST_HEAD(&ring_head);
}

void *ring_calloc(size_t s, char *file, uint64_t line, void *userdata)
{
    void *p = ring_malloc(s, file, line, userdata);
    if (p != NULL) {
        memset(p, 0, s);
    }
    return p;
}

void *ring_malloc(size_t size, char *file, uint64_t line, void *userdata)
{
    void *p = NULL;
    ring_mem_t *mem = malloc(sizeof(ring_mem_t));
    if (mem != NULL) {
        p = malloc(size);
        if (p == NULL) {
            free(mem);
            return NULL;
        }
        mem->p = p;
        mem->s = size;
        mem->file = file;
        mem->line = line;
        mem->userdata = userdata;
        list_add(&mem->ring, &ring_head);
    }
    return p;
}

void *ring_realloc(void *p, size_t size, char *file, uint64_t line, void *userdata)
{
    if (p == NULL) {
        return ring_malloc(size, file, line, userdata);
    } else if (size == 0) {
        ring_free(p);
    } else {
        struct list_head *pos;
        void *_p = realloc(p, size);
        if (_p != NULL && !list_empty(&ring_head)) {
            list_for_each(pos, &ring_head) {
                ring_mem_t *mem = list_entry(pos, ring_mem_t, ring);
                if (mem->p == p) {
                    mem->p = _p;
                    mem->s = size;
                    mem->file = file;
                    mem->line = line;
                    mem->userdata = userdata;
                }
            }
        }
        // safe call for non-safe-mem point.
        return _p;
    }
    return NULL;
}

void ring_free(void *p)
{
    if (p) {
        struct list_head *pos;
        if (!list_empty(&ring_head)) {
            list_for_each(pos, &ring_head) {
                ring_mem_t *mem = list_entry(pos, ring_mem_t, ring);
                if (mem->p == p) {
                    list_del(pos);
                    free(mem);
                    break;
                }
            }
        }
        // let ring_free safety for non-safe-mem point.
        free(p);
    }
}

void ring_clean_check(safe_mem_check_cb cb, void *cb_userdata)
{
    struct list_head *pos, *tmp;
    if (!list_empty(&ring_head)) {
        list_for_each_safe(pos, tmp, &ring_head) {
            ring_mem_t *mem = list_entry(pos, ring_mem_t, ring);
            if (cb) {
                cb(mem->p, mem->s, mem->file, mem->line, mem->userdata, cb_userdata);
            }
            list_del(pos);
            free(mem);
        }
    }
}