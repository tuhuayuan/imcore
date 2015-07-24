/* parser.c
 * 封装expat库
 */
#include "xmpp-inl.h"
#include <expat.h>

#define NAMESPACE_SEP ('\xFF')

struct _parser_t {

    xmpp_conn_t *conn;
    XML_Parser expat;

    parser_start_callback startcb;
    parser_end_callback endcb;
    parser_stanza_callback stanzacb;

    void *userdata;
    int depth;
    xmpp_stanza_t *stanza;

    int reset;
};

static inline void disconnect_parser_error(xmpp_conn_t *conn)
{
    xmpp_error(conn->ctx, "xmpp", "Parser error");
    xmpp_disconnect(conn);
}

#define PARSER_ERROR_RETURN(conn) disconnect_parser_error(conn); return;

static char *_xml_name(xmpp_ctx_t *ctx, const char *nsname)
{
    char *result = NULL;
    const char *c;
    int len;

    c = strchr(nsname, NAMESPACE_SEP);
    if (c == NULL)
        return xmpp_strdup(ctx, nsname);

    c++;
    len = strlen(c);
    result = xmpp_alloc(ctx, len + 1);
    if (result != NULL) {
        memcpy(result, c, len);
        result[len] = '\0';
    }

    return result;
}

static char *_xml_namespace(xmpp_ctx_t *ctx, const char *nsname)
{
    char *result = NULL;
    const char *c;

    c = strchr(nsname, NAMESPACE_SEP);
    if (c != NULL) {
        result = xmpp_alloc(ctx, (c-nsname) + 1);
        if (result != NULL) {
            memcpy(result, nsname, (c-nsname));
            result[c-nsname] = '\0';
        }
    }

    return result;
}

static void _set_attributes(xmpp_stanza_t *stanza, const XML_Char **attrs)
{
    char *attr;
    int i;

    if (!attrs)
        return;

    for (i = 0; attrs[i]; i += 2) {
        attr = _xml_name(stanza->ctx, attrs[i]);
        xmpp_stanza_set_attribute(stanza, attr, attrs[i+1]);
        xmpp_free(stanza->ctx, attr);
    }
}

// expat回调
static void _start_element(void *userdata, const XML_Char *nsname, const XML_Char **attrs)
{
    parser_t *parser = (parser_t *)userdata;
    xmpp_stanza_t *child;
    char *ns, *name;

    // 把namespace分离
    ns = _xml_namespace(parser->conn->ctx, nsname);
    name = _xml_name(parser->conn->ctx, nsname);

    if (parser->depth == 0) {
        // xml流第一层
        if (parser->startcb) {
            parser->startcb((char *)name, (char **)attrs, parser->userdata);
        }

    } else {
        // xml流大于等于第二层
        if (!parser->stanza && parser->depth == 1) {
            parser->stanza = xmpp_stanza_new(parser->conn->ctx);
            if (!parser->stanza) {
                PARSER_ERROR_RETURN(parser->conn);
            }
            xmpp_stanza_set_name(parser->stanza, name);
            _set_attributes(parser->stanza, attrs);
            if (ns)
                xmpp_stanza_set_ns(parser->stanza, ns);

        } else if (parser->depth > 1 && parser->stanza) {
            child = xmpp_stanza_new(parser->conn->ctx);
            if (!child) {
                PARSER_ERROR_RETURN(parser->conn);
            }
            xmpp_stanza_set_name(child, name);
            _set_attributes(child, attrs);
            if (ns)
                xmpp_stanza_set_ns(child, ns);

            xmpp_stanza_add_child(parser->stanza, child);
            xmpp_stanza_release(child);
            parser->stanza = child;
        } else {
            PARSER_ERROR_RETURN(parser->conn);
        }
    }

    if (ns) xmpp_free(parser->conn->ctx, ns);
    if (name) xmpp_free(parser->conn->ctx, name);

    parser->depth++;
}

// expat回调
static void _end_element(void *userdata, const XML_Char *name)
{
    parser_t *parser = (parser_t *)userdata;
    parser->depth--;

    if (parser->depth == 0) {
        // xmp流结束
        if (parser->endcb)
            parser->endcb((char *)name, parser->userdata);

    } else {
        if (parser->stanza->parent) {
            // 子stanza结束，回退到他父元素
            parser->stanza = parser->stanza->parent;
        } else {
            if (parser->stanzacb)
                parser->stanzacb(parser->stanza, parser->userdata);

            xmpp_stanza_release(parser->stanza);
            parser->stanza = NULL;
        }
    }
}

// expat回调
static void _characters(void *userdata, const XML_Char *s, int len)
{
    parser_t *parser = (parser_t*)userdata;
    xmpp_stanza_t *stanza;

    // 不应该在顶层出现text
    if (parser->depth < 2) {
        PARSER_ERROR_RETURN(parser->conn);
    }

    stanza = xmpp_stanza_new(parser->conn->ctx);
    if (!stanza) {
        PARSER_ERROR_RETURN(parser->conn);
    }
    xmpp_stanza_set_text_safe(stanza, s, len);

    xmpp_stanza_add_child(parser->stanza, stanza);
    xmpp_stanza_release(stanza);
}

// 新建一个解析器
parser_t *parser_new(xmpp_conn_t *conn,
                     parser_start_callback startcb,
                     parser_end_callback endcb,
                     parser_stanza_callback stanzacb,
                     void *userdata)
{
    parser_t *parser;

    parser = xmpp_alloc(conn->ctx, sizeof(parser_t));
    if (parser != NULL) {
        parser->conn = conn;
        parser->expat = NULL;
        parser->startcb = startcb;
        parser->endcb = endcb;
        parser->stanzacb = stanzacb;
        parser->userdata = userdata;
        parser->depth = 0;
        parser->stanza = NULL;
        parser->reset = 0;
        parser_reset(parser);
    }

    return parser;
}

// 释放解析器
void parser_free(parser_t *parser)
{
    if (parser->expat)
        XML_ParserFree(parser->expat);

    xmpp_free(parser->conn->ctx, parser);
}

// 重置解析器状态
static void _defer_parser_reset(parser_t *parser)
{
    if (parser->expat)
        XML_ParserFree(parser->expat);

    if (parser->stanza)
        xmpp_stanza_release(parser->stanza);

    parser->expat = XML_ParserCreateNS(NULL, NAMESPACE_SEP);
    if (!parser->expat) {
        PARSER_ERROR_RETURN(parser->conn);
    }

    parser->depth = 0;
    parser->stanza = NULL;

    XML_SetUserData(parser->expat, parser);
    XML_SetElementHandler(parser->expat, _start_element, _end_element);
    XML_SetCharacterDataHandler(parser->expat, _characters);

    parser->reset = 0;
}

// 延迟调用
int parser_reset(parser_t *parser)
{
    parser->reset = 1;
    return 1;
}

// 解析xml流字符串
int parser_feed(parser_t *parser, char *chunk, int len)
{
    if (parser->reset) {
        _defer_parser_reset(parser);
    }
    return XML_Parse(parser->expat, chunk, len, 0);
}

