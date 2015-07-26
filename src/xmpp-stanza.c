/* stanza.c
 * xmpp stanza实现
 * 基本不需要修改，可以添加针对要用的stanza增加helper方法
 */
#include "xmpp-inl.h"

xmpp_stanza_t *xmpp_stanza_new(xmpp_ctx_t *ctx)
{
    xmpp_stanza_t *stanza;
    stanza = xmpp_alloc(ctx, sizeof(xmpp_stanza_t));
    if (stanza != NULL) {
        stanza->ref = 1;
        stanza->ctx = ctx;
        stanza->type = XMPP_STANZA_UNKNOWN;
        stanza->prev = NULL;
        stanza->next = NULL;
        stanza->children = NULL;
        stanza->parent = NULL;
        stanza->data = NULL;
        stanza->attributes = NULL;
    }
    return stanza;
}

// 引用拷贝
xmpp_stanza_t *xmpp_stanza_clone(xmpp_stanza_t *stanza)
{
    stanza->ref++;
    return stanza;
}

// 深度拷贝
xmpp_stanza_t *xmpp_stanza_copy(const xmpp_stanza_t *stanza)
{
    xmpp_stanza_t *copy, *child, *copychild, *tail;
    hash_iterator_t *iter;
    const char *key;
    void *val;
    
    copy = xmpp_stanza_new(stanza->ctx);
    if (!copy)
        goto copy_error;
        
    // 拷贝类型
    copy->type = stanza->type;
    
    // 数据
    if (stanza->data) {
        copy->data = xmpp_strdup(stanza->ctx, stanza->data);
        if (!copy->data) goto copy_error;
    }
    
    // 属性
    if (stanza->attributes) {
        copy->attributes = hash_new(8, xmpp_hash_free);
        if (!copy->attributes) goto copy_error;
        iter = hash_iter_new(stanza->attributes);
        if (!iter) {
            goto copy_error;
        }
        while ((key = hash_iter_next(iter))) {
            val = xmpp_strdup(stanza->ctx,
                              (char *)hash_get(stanza->attributes, key));
                              
            if (!val)
                goto copy_error;
                
            if (hash_add(copy->attributes, key, val))
                goto copy_error;
        }
        hash_iter_release(iter);
    }
    
    // 递归复制所有child
    tail = copy->children;
    for (child = stanza->children; child; child = child->next) {
        copychild = xmpp_stanza_copy(child);
        
        if (!copychild)
            goto copy_error;
            
        copychild->parent = copy;
        if (tail) {
            copychild->prev = tail;
            tail->next = copychild;
        } else {
            copy->children = copychild;
        }
        tail = copychild;
    }
    
    return copy;
    
    // 错误
copy_error:
    if (copy) xmpp_stanza_release(copy);
    return NULL;
}

int xmpp_stanza_release(xmpp_stanza_t *stanza)
{
    int released = 0;
    xmpp_stanza_t *child, *tchild;
    
    // 引用计数判断
    if (stanza->ref > 1)
        stanza->ref--;
    else {
        // 递归调用xmpp_stanza_release删除所有子stanza
        child = stanza->children;
        while (child) {
            tchild = child;
            child = child->next;
            xmpp_stanza_release(tchild);
        }
        if (stanza->attributes) hash_release(stanza->attributes);
        if (stanza->data) xmpp_free(stanza->ctx, stanza->data);
        xmpp_free(stanza->ctx, stanza);
        released = 1;
    }
    return released;
}

int xmpp_stanza_is_text(xmpp_stanza_t *stanza)
{
    return (stanza && stanza->type == XMPP_STANZA_TEXT);
}

int xmpp_stanza_is_tag(xmpp_stanza_t *stanza)
{
    return (stanza && stanza->type == XMPP_STANZA_TAG);
}

static char *_escape_xml(xmpp_ctx_t *ctx, char *text)
{
    size_t len = 0;
    char *src;
    char *dst;
    char *buf;
    for (src = text; *src != '\0'; src++) {
        switch (*src) {
        case '<':   /* "&lt;" */
        case '>':   /* "&gt;" */
            len += 4;
            break;
        case '&':   /* "&amp;" */
            len += 5;
            break;
        case '"':
            len += 6; /*"&quot;" */
            break;
        default:
            len++;
        }
    }
    if ((buf = xmpp_alloc(ctx, (len+1) * sizeof(char))) == NULL)
        return NULL;    /* Error */
    dst = buf;
    for (src = text; *src != '\0'; src++) {
        switch (*src) {
        case '<':
            strcpy(dst, "&lt;");
            dst += 4;
            break;
        case '>':
            strcpy(dst, "&gt;");
            dst += 4;
            break;
        case '&':
            strcpy(dst, "&amp;");
            dst += 5;
            break;
        case '"':
            strcpy(dst, "&quot;");
            dst += 6;
            break;
        default:
            *dst = *src;
            dst++;
        }
    }
    *dst = '\0';
    return buf;
}

static inline void _render_update(int *written, int length, int lastwrite, size_t *left, char **ptr)
{
    *written += lastwrite;
    if (*written > length) {
        *left = 0;
        *ptr = NULL;
    } else {
        *left -= lastwrite;
        *ptr = &(*ptr)[lastwrite];
    }
}

// 递归序列化
static int _render_stanza_recursive(xmpp_stanza_t *stanza, char *buf, size_t buflen)
{
    int ret, written;
    
    char *ptr = buf;
    size_t left = buflen;
    
    xmpp_stanza_t *child;
    hash_iterator_t *iter;
    
    const char *key;
    char *tmp;
    char *value;
    
    // 已经写了字符数
    written = 0;
    
    if (stanza->type == XMPP_STANZA_UNKNOWN)
        return XMPP_EINVOP;
        
    if (stanza->type == XMPP_STANZA_TEXT) {
        if (!stanza->data) {
            return XMPP_EINVOP;
        }
        
        // xml字符转换
        tmp = _escape_xml(stanza->ctx, stanza->data);
        if (tmp == NULL)
            return XMPP_EMEM;
            
        ret = im_snprintf(ptr, left, "%s", tmp);
        xmpp_free(stanza->ctx, tmp);
        if (ret < 0)
            return XMPP_EMEM;
        _render_update(&written, buflen, ret, &left, &ptr);
        
    } else {
        if (!stanza->data)
            return XMPP_EINVOP;
            
        // 输出xml标签头
        ret = im_snprintf(ptr, left, "<%s", stanza->data);
        if (ret < 0)
            return XMPP_EMEM;
        _render_update(&written, buflen, ret, &left, &ptr);
        
        // 输出标签属性
        if (stanza->attributes && hash_num_keys(stanza->attributes) > 0) {
            iter = hash_iter_new(stanza->attributes);
            while ((key = hash_iter_next(iter))) {
                value = (char*)hash_get(stanza->attributes, key);
                if (!value)
                    continue;
                    
                // 处理xmlns属性
                if (!strcmp(key, "xmlns")) {
                
                    if (stanza->parent && stanza->parent->attributes) {
                        char *parent_key_value = (char*)hash_get(stanza->parent->attributes, key);
                        if (parent_key_value && !strcmp(value, parent_key_value)) {
                            continue;
                        }
                    }
                    
                    if (!stanza->parent && !strcmp(value, XMPP_NS_CLIENT))
                        continue;
                }
                
                tmp = _escape_xml(stanza->ctx, value);
                if (tmp == NULL)
                    return XMPP_EMEM;
                    
                // 输出并更新索引
                ret = im_snprintf(ptr, left, " %s='%s'", key, tmp);
                xmpp_free(stanza->ctx, tmp);
                if (ret < 0)
                    return XMPP_EMEM;
                _render_update(&written, buflen, ret, &left, &ptr);
            }
            hash_iter_release(iter);
        }
        if (!stanza->children) {
            // 没有子元素则关闭标签
            ret = im_snprintf(ptr, left, "/>");
            if (ret < 0)
                return XMPP_EMEM;
            _render_update(&written, buflen, ret, &left, &ptr);
        } else {
            // 输出起始标签结束
            ret = im_snprintf(ptr, left, ">");
            if (ret < 0)
                return XMPP_EMEM;
            _render_update(&written, buflen, ret, &left, &ptr);
            
            // 循环输出子元素
            child = stanza->children;
            while (child) {
                ret = _render_stanza_recursive(child, ptr, left);
                if (ret < 0)
                    return ret;
                _render_update(&written, buflen, ret, &left, &ptr);
                child = child->next;
            }
            
            // 输出结束标签
            ret = im_snprintf(ptr, left, "</%s>", stanza->data);
            if (ret < 0)
                return XMPP_EMEM;
            _render_update(&written, buflen, ret, &left, &ptr);
        }
    }
    return written;
}

// xml stanza 转 字符串
int  xmpp_stanza_to_text(xmpp_stanza_t *stanza, char **const buf, size_t *buflen)
{
    char *buffer, *tmp;
    size_t length;
    int ret;
    
    // 单位缓冲区大小
    length = 128;
    buffer = xmpp_alloc(stanza->ctx, length);
    if (!buffer) {
        *buf = NULL;
        *buflen = 0;
        return XMPP_EMEM;
    }
    
    ret = _render_stanza_recursive(stanza, buffer, length);
    if (ret < 0)
        return ret;
        
    // 缓冲区不够大，再分配一个单位大小，之前的解析内容保留
    if ((size_t)ret > length - 1) {
        tmp = xmpp_realloc(stanza->ctx, buffer, ret + 1);
        if (!tmp) {
            xmpp_free(stanza->ctx, buffer);
            *buf = NULL;
            *buflen = 0;
            return XMPP_EMEM;
        }
        length = ret + 1;
        buffer = tmp;
        ret = _render_stanza_recursive(stanza, buffer, length);
        if ((size_t)ret > length - 1)
            return XMPP_EMEM;
    }
    
    buffer[length - 1] = 0;
    *buf = buffer;
    *buflen = ret;
    return XMPP_EOK;
}

int xmpp_stanza_set_name(xmpp_stanza_t *stanza, const char *name)
{
    if (stanza->type == XMPP_STANZA_TEXT) return XMPP_EINVOP;
    if (stanza->data) xmpp_free(stanza->ctx, stanza->data);
    stanza->type = XMPP_STANZA_TAG;
    stanza->data = xmpp_strdup(stanza->ctx, name);
    return XMPP_EOK;
}

char *xmpp_stanza_get_name_ptr(xmpp_stanza_t *stanza)
{
    if (stanza->type == XMPP_STANZA_TEXT) return NULL;
    return stanza->data;
}

int xmpp_stanza_get_attribute_count(xmpp_stanza_t *stanza)
{
    if (stanza->attributes == NULL) {
        return 0;
    }
    return hash_num_keys(stanza->attributes);
}

int xmpp_stanza_get_attributes(xmpp_stanza_t *stanza, const char **attr, int attrlen)
{
    hash_iterator_t *iter;
    const char *key;
    int num = 0;
    if (stanza->attributes == NULL) {
        return 0;
    }
    iter = hash_iter_new(stanza->attributes);
    while ((key = hash_iter_next(iter)) != NULL && attrlen) {
        attr[num++] = key;
        attrlen--;
        if (attrlen == 0) {
            hash_iter_release(iter);
            return num;
        }
        attr[num++] = hash_get(stanza->attributes, key);
        attrlen--;
        if (attrlen == 0) {
            hash_iter_release(iter);
            return num;
        }
    }
    hash_iter_release(iter);
    return num;
}

int xmpp_stanza_set_attribute(xmpp_stanza_t *stanza, const char *key, const char *value)
{
    char *val;
    if (stanza->type != XMPP_STANZA_TAG) {
        return XMPP_EINVOP;
    }
    
    if (!stanza->attributes) {
        stanza->attributes = hash_new(8, xmpp_hash_free);
        if (!stanza->attributes) return XMPP_EMEM;
    }
    
    val = xmpp_strdup(stanza->ctx, value);
    if (!val) {
        return XMPP_EMEM;
    }
    
    hash_add(stanza->attributes, key, val);
    return XMPP_EOK;
}

int xmpp_stanza_set_ns(xmpp_stanza_t *stanza, const char *ns)
{
    return xmpp_stanza_set_attribute(stanza, "xmlns", ns);
}

int xmpp_stanza_add_child(xmpp_stanza_t *stanza, xmpp_stanza_t *child)
{
    xmpp_stanza_t *s;
    
    // 添加引用计数
    xmpp_stanza_clone(child);
    
    child->parent = stanza;
    if (!stanza->children) {
        stanza->children = child;
    } else {
        s = stanza->children;
        while (s->next) s = s->next;
        s->next = child;
        child->prev = s;
    }
    return XMPP_EOK;
}

int xmpp_stanza_set_text(xmpp_stanza_t *stanza, const char *text)
{
    if (stanza->type == XMPP_STANZA_TAG)
        return XMPP_EINVOP;
        
    stanza->type = XMPP_STANZA_TEXT;
    if (stanza->data)
        xmpp_free(stanza->ctx, stanza->data);
    stanza->data = xmpp_strdup(stanza->ctx, text);
    
    return XMPP_EOK;
}

int xmpp_stanza_set_text_safe(xmpp_stanza_t *stanza, const char *text, size_t size)
{
    if (stanza->type == XMPP_STANZA_TAG)
        return XMPP_EINVOP;
    stanza->type = XMPP_STANZA_TEXT;
    
    // 释放之前的
    if (stanza->data)
        xmpp_free(stanza->ctx, stanza->data);
        
    stanza->data = xmpp_alloc(stanza->ctx, size + 1);
    if (!stanza->data)
        return XMPP_EMEM;
        
    memcpy(stanza->data, text, size);
    stanza->data[size] = 0;
    return XMPP_EOK;
}

char *xmpp_stanza_get_id_ptr(xmpp_stanza_t *stanza)
{
    if (stanza->type != XMPP_STANZA_TAG)
        return NULL;
    if (!stanza->attributes)
        return NULL;
    return (char *)hash_get(stanza->attributes, "id");
}

const char * xmpp_stanza_get_ns(xmpp_stanza_t *stanza)
{
    if (stanza->type != XMPP_STANZA_TAG)
        return NULL;
    if (!stanza->attributes)
        return NULL;
    return (char *)hash_get(stanza->attributes, "xmlns");
}

char *xmpp_stanza_get_type_ptr(xmpp_stanza_t *stanza)
{
    if (stanza->type != XMPP_STANZA_TAG)
        return NULL;
    if (!stanza->attributes)
        return NULL;
    return (char *)hash_get(stanza->attributes, "type");
}

xmpp_stanza_t *xmpp_stanza_get_child_by_name(xmpp_stanza_t *stanza, const char *name)
{
    xmpp_stanza_t *child = NULL;
    for (child = stanza->children; child; child = child->next) {
        if (child->type == XMPP_STANZA_TAG &&
            (strcmp(name, xmpp_stanza_get_name_ptr(child)) == 0))
            break;
    }
    return child;
}

xmpp_stanza_t *xmpp_stanza_get_child_by_ns(xmpp_stanza_t *stanza, const char *ns)
{
    xmpp_stanza_t *child = NULL;
    for (child = stanza->children; child; child = child->next) {
        if (xmpp_stanza_get_ns(child) &&
            strcmp(ns, xmpp_stanza_get_ns(child)) == 0)
            break;
    }
    return child;
}

xmpp_stanza_t *xmpp_stanza_get_children(xmpp_stanza_t *stanza)
{
    return stanza->children;
}

xmpp_stanza_t *xmpp_stanza_get_next(xmpp_stanza_t *stanza)
{
    return stanza->next;
}

char *xmpp_stanza_get_text_ptr(xmpp_stanza_t *stanza)
{
    if (stanza->type == XMPP_STANZA_TAG) {
        stanza = stanza->children;
    }
    if (stanza && stanza->type == XMPP_STANZA_TEXT)
        return stanza->data;
    return NULL;
}

int xmpp_stanza_set_id(xmpp_stanza_t *stanza, const char *id)
{
    return xmpp_stanza_set_attribute(stanza, "id", id);
}

int xmpp_stanza_set_type(xmpp_stanza_t *stanza, const char *type)
{
    return xmpp_stanza_set_attribute(stanza, "type", type);
}

const char * xmpp_stanza_get_attribute(xmpp_stanza_t *stanza, const char *name)
{
    if (stanza->type != XMPP_STANZA_TAG)
        return NULL;
    if (!stanza->attributes)
        return NULL;
    return hash_get(stanza->attributes, name);
}
