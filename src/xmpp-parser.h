/* parser.h
 * ½âÎöÆ÷
*/

#ifndef __IMCORE_XMPP_PARSER_H__
#define __IMCORE_XMPP_PARSER_H__

#include "xmpp-inl.h"

typedef struct _parser_t parser_t;

typedef void (*parser_start_callback)(char *name, char **attrs, void *userdata);
typedef void (*parser_end_callback)(char *name, void *userdata);
typedef void (*parser_stanza_callback)(xmpp_stanza_t *stanza, void *userdata);

parser_t *parser_new(xmpp_conn_t *conn, parser_start_callback startcb,
                     parser_end_callback endcb, parser_stanza_callback stanzacb, void *userdata);
void parser_free(parser_t *parser);
int parser_reset(parser_t *parser);
int parser_feed(parser_t *parser, char *chunk, int len);

#endif // __IMCORE_XMPP_PARSER_H__
