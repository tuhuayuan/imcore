/* conn.c
 * ���ӹ���
 */
#include "xmpp-inl.h"

#define RESPOND_TIMEOUT 5                    //Ĭ��5�볬ʱ


// ����������Timer callback
static int _disconnect_cleanup(xmpp_conn_t *conn, void *userdata);

// Parser �ص�
static void _handle_stream_start(char *name, char **attrs, void *userdata);
static void _handle_stream_end(char *name, void *userdata);
static void _handle_stream_stanza(xmpp_stanza_t *stanza, void *userdata);

// ��ȡ���ݻص�
static void _evb_read_cb(struct bufferevent *bev, void *ptr)
{
    char buff[4096];
    xmpp_conn_t *conn = ptr;
    int ret = 0;
    size_t len = 0;
    
    if ((len = bufferevent_read(conn->evbuffer, buff, sizeof(buff))) > 0) {
        // ���������������
        ret = parser_feed(conn->parser, buff, len);
        if (!ret) {
            // xml����������
            xmpp_debug(conn->ctx, "xmpp", "XML parse error.");
            conn_do_disconnect(conn);
        }
    }
}

// �¼��ص�
static void _evb_event_cb(struct bufferevent *bev, short what, void *ptr)
{
    xmpp_conn_t *conn = ptr;
    if (what & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
        // �����Լ��Ͽ�����
        conn->error = ECONNRESET;
        xmpp_info(conn->ctx, "xmpp", "Connection closed by remote host.");
        conn_do_disconnect(conn);
        
    } else if (what & BEV_EVENT_TIMEOUT) {
        // ��ʱ����
        conn->error = ETIMEDOUT;
        xmpp_info(conn->ctx, "xmpp", "Connection timed out.");
        conn_do_disconnect(conn);
        
    } else if (what & BEV_EVENT_CONNECTED) {
        if (conn->state == XMPP_STATE_CONNECTING) {
            conn->state = XMPP_STATE_CONNECTED;
            xmpp_debug(conn->ctx, "xmpp", "Connection successful");
            if (bufferevent_openssl_get_ssl(bev) != NULL) {
                conn->secured = 1;
            }
            
            // ����buff�ص�
            bufferevent_enable(bev, EV_READ | EV_WRITE);
            bufferevent_setcb(conn->evbuffer, _evb_read_cb, NULL, _evb_event_cb, conn);
            
            // ��ʼ����
            conn_reset_stream(conn, auth_handle_open);
            conn_init_stream(conn);
        }
    }
}

xmpp_conn_t *xmpp_conn_new(xmpp_ctx_t *ctx)
{
    xmpp_conn_t *conn = NULL;
    
    if (ctx == NULL)
        return NULL;
        
    // ����ṹ�ڴ�
    conn = xmpp_alloc(ctx, sizeof(xmpp_conn_t));
    if (conn != NULL) {
        conn->ctx = ctx;
        conn->type = XMPP_UNKNOWN;
        conn->state = XMPP_STATE_DISCONNECTED;
        conn->error = 0;
        conn->stream_error = NULL;
        conn->respond_timeout = RESPOND_TIMEOUT;
        conn->userdata = NULL;
        conn->evbuffer = NULL;
        
        // ������Ϣ
        conn->lang = xmpp_strdup(conn->ctx, "zh-cn");
        if (!conn->lang) {
            xmpp_free(conn->ctx, conn);
            return NULL;
        }
        conn->domain = NULL;
        conn->jid = NULL;
        conn->pass = NULL;
        conn->stream_id = NULL;
        conn->bound_jid = NULL;
        conn->tls_support = 0;
        conn->tls_disabled = 0;
        conn->tls_failed = 0;
        conn->sasl_support = 0;
        conn->secured = 0;
        conn->bind_required = 0;
        conn->session_required = 0;
        
        // ������
        conn->parser = parser_new(conn,
                                  _handle_stream_start,
                                  _handle_stream_end,
                                  _handle_stream_stanza,
                                  conn);
                                  
        // ����xmpp��֤���
        conn_reset_stream(conn, auth_handle_open);
        conn->authenticated = 0;
        
        // ����handler
        conn->conn_handler = NULL;
        
        //hash���ڲ�Ҳ��xmpp_handlist_t
        conn->id_handlers = hash_new(conn->ctx, 32, NULL);
        
        // handler����ͷ
        INIT_LIST_HEAD(&conn->timed_handlers.dlist);
        INIT_LIST_HEAD(&conn->handlers.dlist);
        
        // ���ü���
        conn->ref = 1;
        
    }
    return conn;
}

xmpp_conn_t *xmpp_conn_clone(xmpp_conn_t *conn)
{
    conn->ref++;
    return conn;
}

int xmpp_conn_release(xmpp_conn_t *conn)
{
    xmpp_ctx_t *ctx = NULL;
    int released = 0;
    
    if (conn->ref > 1) {
        // ���ü���û��0
        conn->ref--;
    } else {
        ctx = conn->ctx;
        
        handler_clear_all(conn);
        
        // �ͷŴ���stanza
        if (conn->stream_error) {
            xmpp_stanza_release(conn->stream_error->stanza);
            if (conn->stream_error->text)
                xmpp_free(ctx, conn->stream_error->text);
            xmpp_free(ctx, conn->stream_error);
        }
        
        // �ͷŽ�����
        parser_free(conn->parser);
        
        // �ͷŸ����ַ���
        if (conn->domain) xmpp_free(ctx, conn->domain);
        if (conn->jid) xmpp_free(ctx, conn->jid);
        if (conn->bound_jid) xmpp_free(ctx, conn->bound_jid);
        if (conn->pass) xmpp_free(ctx, conn->pass);
        if (conn->stream_id) xmpp_free(ctx, conn->stream_id);
        if (conn->lang) xmpp_free(ctx, conn->lang);
        
        // �ɾ���
        xmpp_free(ctx, conn);
        released = 1;
    }
    return released;
}

int xmpp_connect_client(xmpp_conn_t *conn, const char *altdomain, unsigned short altport,
                        xmpp_conn_handler callback, void *userdata)
{
    char connectdomain[2048];
    int connectport;
    const char *domain;
    conn->type = XMPP_CLIENT;
    
    // ��ȡjid���������, jid����������Ҫ��ѯSRV��¼���ʵ�ʵķ���������
    conn->domain = xmpp_jid_domain(conn->ctx, conn->jid);
    if (!conn->domain)
        return -1;
        
    // ֱ��ָ���˷��������������Ƕ�jid����������SRV����
    if (altdomain) {
        xmpp_debug(conn->ctx, "xmpp", "Connecting via altdomain.");
        strcpy(connectdomain, altdomain);
        connectport = altport ? altport : 5222;
    } else if (!sock_srv_lookup("xmpp-client", "tcp", conn->domain,
                                connectdomain, 2048, &connectport)) {
        xmpp_debug(conn->ctx, "xmpp", "SRV lookup failed.");
        if (!altdomain)
            domain = conn->domain;
        else
            domain = altdomain;
        xmpp_debug(conn->ctx, "xmpp", "Using alternate domain %s, port %d",
                   altdomain, altport);
        strcpy(connectdomain, domain);
        connectport = altport ? altport : 5222;
    }
    
    // ��ʼ��������
    // �˿��ַ�������
    int err;
    char port_buf[6];
    struct evutil_addrinfo hints;
    struct evutil_addrinfo *answer = NULL;
    
    // �Ѷ˿�ת����ʮ���Ƹ�ʽ���ַ���
    evutil_snprintf(port_buf, sizeof(port_buf), "%d", (int)connectport);
    memset(&hints, 0, sizeof(hints));
    
    // ����Э�����
    hints.ai_family = AF_INET; // Э����
    hints.ai_socktype = SOCK_STREAM; // ��socket
    hints.ai_protocol = IPPROTO_TCP; // TCPЭ��
    hints.ai_flags = EVUTIL_AI_ADDRCONFIG; // ֻ����ipv4�ĵ�ַ
    
    // ������ַ
    err = evutil_getaddrinfo(connectdomain, port_buf, &hints, &answer);
    if (err != 0) {
        xmpp_error(conn->ctx, "xmpp", "getaddrinfo returned %d", gai_strerror(err));
        return -1;
    }
    
    // ��ʼ����
    // ����bufferevent
    conn->evbuffer = bufferevent_socket_new(conn->ctx->base, -1,
                                            BEV_OPT_CLOSE_ON_FREE | BEV_OPT_THREADSAFE | BEV_OPT_DEFER_CALLBACKS);
    if (!conn->evbuffer) {
        xmpp_error(conn->ctx, "xmpp", "bufferevent_socket_new returned NULL");
        return -1;
    }
    
    // ���ûص�
    bufferevent_setcb(conn->evbuffer, NULL, NULL, _evb_event_cb, conn);
    
    /* char ipstr[16];
     for (curr = answer; curr != NULL; curr = curr->ai_next) {
         inet_ntop(AF_INET, &((struct sockaddr_in*)(curr->ai_addr))->sin_addr, ipstr, sizeof(ipstr));
         printf("#### %s\n", ipstr);
     }*/
    
    // �����첽����
    if (bufferevent_socket_connect(conn->evbuffer, (struct sockaddr*)answer->ai_addr,
                                   answer->ai_addrlen) < 0) {
        xmpp_error(conn->ctx, "xmpp", "bufferevent_socket_connect error");
        bufferevent_free(conn->evbuffer);
        return -1;
    }
    
    // �������ӻص�
    conn->conn_handler = callback;              // �ⲿ�ӿ�
    conn->userdata = userdata;
    
    conn->state = XMPP_STATE_CONNECTING;
    xmpp_debug(conn->ctx, "xmpp", "attempting to connect to %s", connectdomain);
    
    evutil_freeaddrinfo(answer);
    return 0;
}

// ����ssl����
void conn_start_ssl(xmpp_conn_t *conn)
{
    struct event_base *base = conn->ctx->base;
    struct bufferevent* ssl_bev = NULL;
    SSL *ssl = NULL;
    
    // �������ѽ�����TCP���Ӳ�����������
    if (conn->state != XMPP_STATE_CONNECTED
        || (ssl = bufferevent_openssl_get_ssl(conn->evbuffer)))
        return;
        
    ssl = SSL_new(conn->ctx->ssl_ctx);
    ssl_bev = bufferevent_openssl_filter_new(base, conn->evbuffer,
              ssl, BUFFEREVENT_SSL_CONNECTING,
              BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS);
              
    if (!ssl_bev) {
        // ����opensll������ʧ��
        xmpp_error(conn->ctx, "conn", "Open tls bufferevent failed.");
        conn_do_disconnect(conn);
        
    } else {
        conn->state = XMPP_STATE_CONNECTING;
        conn->evbuffer = ssl_bev;
        bufferevent_setcb(conn->evbuffer, NULL, NULL, _evb_event_cb, conn);
    }
    
}

// ����ѹ������
int conn_start_compression(xmpp_conn_t *conn)
{
    return -1;
}

void conn_do_disconnect(xmpp_conn_t *conn)
{
    xmpp_debug(conn->ctx, "xmpp", "Closing socket.");
    
    // ɾ����ʱ��
    xmpp_timed_handler_delete(conn, _disconnect_cleanup);
    
    // ����״̬
    conn->state = XMPP_STATE_DISCONNECTED;
    
    // �ͷ�����
    bufferevent_free(conn->evbuffer);
    
    // ֪ͨ�ⲿӦ�ó���
    conn->conn_handler(conn, XMPP_CONN_DISCONNECT, conn->error,
                       conn->stream_error, conn->userdata);
}

void conn_reset_stream(xmpp_conn_t *conn, xmpp_open_handler handler)
{
    conn->open_handler = handler;
    parser_reset(conn->parser);
}

// �ճ�ʱtimer�ص�
static int _disconnect_cleanup(xmpp_conn_t *conn, void *userdata)
{
    xmpp_debug(conn->ctx, "xmpp", "disconnection forced by cleanup timeout");
    conn_do_disconnect(conn);
    
    return XMPP_HANDLER_END;
}

void xmpp_disconnect(xmpp_conn_t *conn)
{
    if (conn->state != XMPP_STATE_CONNECTING &&
        conn->state != XMPP_STATE_CONNECTED)
        return;
        
    // �������ر�stanza
    xmpp_send_raw_string(conn, "</stream:stream>");
    
    // �����رճ�ʱtimer
    handler_add_timed(conn, _disconnect_cleanup, RESPOND_TIMEOUT, NULL);
}

void xmpp_send_raw_string(xmpp_conn_t *conn, const char *fmt, ...)
{
    va_list ap;
    size_t len;
    char buf[1024]; // Ĭ�ϻ���
    char *bigbuf;
    
    va_start(ap, fmt);
    len = imcore_vsnprintf(buf, 1024, fmt, ap);
    va_end(ap);
    
    //
    if (len >= 1024) {
        // ���Ȳ��������ڶ��������
        len++;
        bigbuf = xmpp_alloc(conn->ctx, len);
        if (!bigbuf) {
            xmpp_debug(conn->ctx, "xmpp", "Could not allocate memory for send_raw_string");
            return;
        }
        va_start(ap, fmt);
        imcore_vsnprintf(bigbuf, len, fmt, ap);
        va_end(ap);
        
        xmpp_debug(conn->ctx, "conn", "SENT: %s", bigbuf);
        xmpp_send_raw(conn, bigbuf, len - 1); // -1
        xmpp_free(conn->ctx, bigbuf);
        
    } else {
        xmpp_debug(conn->ctx, "conn", "SENT: %s", buf);
        xmpp_send_raw(conn, buf, len);
    }
}

void xmpp_send_raw(xmpp_conn_t *conn, const char *data, size_t len)
{
    if (conn->state != XMPP_STATE_CONNECTED)
        return;
        
    if (bufferevent_write(conn->evbuffer, data, len) < 0) {
        // д������ʧ��
        xmpp_error(conn->ctx, "conn", "Write to bufferevent failed.");
        conn_do_disconnect(conn);
    }
}

void xmpp_send(xmpp_conn_t *conn, xmpp_stanza_t *stanza)
{
    char *buf;
    size_t len;
    int ret;
    if (conn->state == XMPP_STATE_CONNECTED) {
        if ((ret = xmpp_stanza_to_text(stanza, &buf, &len)) == 0) {
            xmpp_send_raw(conn, buf, len);
            xmpp_debug(conn->ctx, "conn", "SENT: %s", buf);
            xmpp_free(conn->ctx, buf);
        }
    }
}

void conn_init_stream(xmpp_conn_t *conn)
{
    xmpp_send_raw_string(conn,
                         "<?xml version=\"1.0\"?>"      \
                         "<stream:stream to=\"%s\" "      \
                         "xml:lang=\"%s\" "       \
                         "version=\"1.0\" "       \
                         "xmlns=\"%s\" "        \
                         "xmlns:stream=\"%s\">",
                         conn->domain,
                         conn->lang,
                         conn->type == XMPP_CLIENT ? XMPP_NS_CLIENT : XMPP_NS_COMPONENT,
                         XMPP_NS_STREAMS);
}

// ��ȡָ������ֵ
static char *_get_stream_attribute(char **attrs, char *name)
{
    int i;
    if (!attrs) return NULL;
    for (i = 0; attrs[i]; i += 2)
        if (strcmp(name, attrs[i]) == 0)
            return attrs[i+1];
    return NULL;
}

static void _handle_stream_start(char *name, char **attrs, void *userdata)
{
    xmpp_conn_t *conn = (xmpp_conn_t *)userdata;
    char *id;
    if (strcmp(name, "stream")) {
        //�����������˴��������ͷ
        xmpp_error(conn->ctx, "conn", "Server did not open valid stream.");
        conn_do_disconnect(conn);
        
    } else {
    
        // �ͷ�֮ǰ����ID�ַ���
        if (conn->stream_id)
            xmpp_free(conn->ctx, conn->stream_id);
            
        // �����ID
        id = _get_stream_attribute(attrs, "id");
        if (id)
            conn->stream_id = xmpp_strdup(conn->ctx, id);
            
        // û����ID�Ͽ�����
        if (!conn->stream_id) {
            xmpp_error(conn->ctx, "conn", "No stream id.");
            conn_do_disconnect(conn);
        }
    }
    
    // ֪ͨ������
    conn->open_handler(conn);
}

static void _handle_stream_end(char *name, void *userdata)
{
    xmpp_conn_t *conn = (xmpp_conn_t *)userdata;
    xmpp_debug(conn->ctx, "xmpp", "RECV: </stream:stream>");
    
    // �Ͽ�����
    conn_do_disconnect(conn);
}

static void _handle_stream_stanza(xmpp_stanza_t *stanza, void *userdata)
{
    xmpp_conn_t *conn = (xmpp_conn_t *)userdata;
#ifdef _DEBUG
    char *buf;
    size_t len;
    if (xmpp_stanza_to_text(stanza, &buf, &len) == 0) {
        xmpp_debug(conn->ctx, "xmpp", "RECV: %s", buf);
        xmpp_free(conn->ctx, buf);
    }
#endif // DEBUG
    // ����handler
    handler_fire_stanza(conn, stanza);
}

xmpp_ctx_t* xmpp_conn_get_context(xmpp_conn_t *conn)
{
    return conn->ctx;
}

const char *xmpp_conn_get_jid(const xmpp_conn_t *conn)
{
    return conn->jid;
}

const char *xmpp_conn_get_bound_jid(const xmpp_conn_t *conn)
{
    return conn->bound_jid;
}

void xmpp_conn_set_jid(xmpp_conn_t *conn, const char *jid)
{
    if (conn->jid) xmpp_free(conn->ctx, conn->jid);
    conn->jid = xmpp_strdup(conn->ctx, jid);
}

const char *xmpp_conn_get_pass(const xmpp_conn_t *conn)
{
    return conn->pass;
}

void xmpp_conn_set_timeout(xmpp_conn_t *conn, unsigned long usec)
{
    if (usec > 0) {
        conn->respond_timeout = usec;
    }
}

void xmpp_conn_set_pass(xmpp_conn_t *conn, const char *pass)
{
    if (conn->pass) xmpp_free(conn->ctx, conn->pass);
    conn->pass = xmpp_strdup(conn->ctx, pass);
}

void xmpp_conn_disable_tls(xmpp_conn_t *conn)
{
    conn->tls_disabled = 1;
}