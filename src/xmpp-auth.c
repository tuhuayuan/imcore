/*auth.c
* xmpp握手实现
*/
#include "md5.h"

#include "xmpp-inl.h"

#define XMPP_BIND_ID "imcore_xmpp_bind"
#define XMPP_SESSION_ID "imcore_xmpp_session"
#define XMPP_AUTH_TIMEOUT 5                        // 认证流程5秒超时

// 认证操作超时
static int _handle_auth_timeout(xmpp_conn_t *conn, void *userdata)
{
    xmpp_error(conn->ctx, "xmpp", "Server auth timeout.");
    xmpp_disconnect(conn);
    
    return XMPP_HANDLER_END;
}

// 私有入口
static void _do_auth(xmpp_conn_t *conn);
static void _auth_handle_open_sasl(xmpp_conn_t *conn);

// 前置声明
static int _handle_features_sasl(xmpp_conn_t *conn, xmpp_stanza_t *stanza,
                                 void *userdata);
static int _handle_sasl_result(xmpp_conn_t *conn, xmpp_stanza_t *stanza, void *userdata);
static int _handle_digestmd5_challenge(xmpp_conn_t *conn, xmpp_stanza_t *stanza,
                                       void *userdata);
static int _handle_digestmd5_rspauth(xmpp_conn_t *conn, xmpp_stanza_t *stanza,
                                     void *userdata);
static int _handle_bind(xmpp_conn_t *conn, xmpp_stanza_t *stanza, void *userdata);
static int _handle_session(xmpp_conn_t *conn, xmpp_stanza_t *stanza, void *userdata);
static int _handle_proceedtls(xmpp_conn_t *conn, xmpp_stanza_t *stanza, void *userdata);

// 由于内存分配错误断开连接
static inline void disconnect_mem_error(xmpp_conn_t *conn)
{
    xmpp_error(conn->ctx, "xmpp", "Memory allocation error");
    xmpp_disconnect(conn);
}

// 创建starttls节
static xmpp_stanza_t *_make_starttls(xmpp_conn_t *conn)
{
    xmpp_stanza_t *starttls;
    
    starttls = xmpp_stanza_new(conn->ctx);
    if (starttls) {
        xmpp_stanza_set_name(starttls, "starttls");
        xmpp_stanza_set_ns(starttls, XMPP_NS_TLS);
    }
    
    return starttls;
}

// 创建指定机制的SASL节
static xmpp_stanza_t *_make_sasl_auth(xmpp_conn_t *conn, const char *mechanism)
{
    xmpp_stanza_t *auth;
    
    auth = xmpp_stanza_new(conn->ctx);
    if (auth) {
        xmpp_stanza_set_name(auth, "auth");
        xmpp_stanza_set_ns(auth, XMPP_NS_SASL);
        xmpp_stanza_set_attribute(auth, "mechanism", mechanism);
    }
    
    return auth;
}

// 处理stream中出现的错误
static int _handle_stream_error(xmpp_conn_t *conn, xmpp_stanza_t *stanza, void *userdata)
{
    xmpp_stanza_t *child;
    char *name;
    
    // 释放之前的错误
    if (conn->stream_error) {
        xmpp_stanza_release(conn->stream_error->stanza);
        xmpp_free(conn->ctx, conn->stream_error);
    }
    
    // 生成当前错误
    conn->stream_error = (xmpp_stream_error_t *)xmpp_alloc(conn->ctx, sizeof(xmpp_stream_error_t));
    conn->stream_error->text = NULL;
    conn->stream_error->type = XMPP_SE_UNDEFINED_CONDITION;
    
    if (conn->stream_error) {
        child = xmpp_stanza_get_children(stanza);
        do {
            const char *ns = NULL;
            
            if (child) {
                ns = xmpp_stanza_get_ns(child);
            }
            
            if (ns && strcmp(ns, XMPP_NS_STREAMS_IETF) == 0) {
                name = xmpp_stanza_get_name_ptr(child);
                if (strcmp(name, "text") == 0) {
                    conn->stream_error->text = xmpp_stanza_get_text_ptr(child);
                } else if (strcmp(name, "bad-format") == 0)
                    conn->stream_error->type = XMPP_SE_BAD_FORMAT;
                else if (strcmp(name, "bad-namespace-prefix") == 0)
                    conn->stream_error->type = XMPP_SE_BAD_NS_PREFIX;
                else if (strcmp(name, "conflict") == 0)
                    conn->stream_error->type = XMPP_SE_CONFLICT;
                else if (strcmp(name, "connection-timeout") == 0)
                    conn->stream_error->type = XMPP_SE_CONN_TIMEOUT;
                else if (strcmp(name, "host-gone") == 0)
                    conn->stream_error->type = XMPP_SE_HOST_GONE;
                else if (strcmp(name, "host-unknown") == 0)
                    conn->stream_error->type = XMPP_SE_HOST_UNKNOWN;
                else if (strcmp(name, "improper-addressing") == 0)
                    conn->stream_error->type = XMPP_SE_IMPROPER_ADDR;
                else if (strcmp(name, "internal-server-error") == 0)
                    conn->stream_error->type = XMPP_SE_INTERNAL_SERVER_ERROR;
                else if (strcmp(name, "invalid-from") == 0)
                    conn->stream_error->type = XMPP_SE_INVALID_FROM;
                else if (strcmp(name, "invalid-id") == 0)
                    conn->stream_error->type = XMPP_SE_INVALID_ID;
                else if (strcmp(name, "invalid-namespace") == 0)
                    conn->stream_error->type = XMPP_SE_INVALID_NS;
                else if (strcmp(name, "invalid-xml") == 0)
                    conn->stream_error->type = XMPP_SE_INVALID_XML;
                else if (strcmp(name, "not-authorized") == 0)
                    conn->stream_error->type = XMPP_SE_NOT_AUTHORIZED;
                else if (strcmp(name, "policy-violation") == 0)
                    conn->stream_error->type = XMPP_SE_POLICY_VIOLATION;
                else if (strcmp(name, "remote-connection-failed") == 0)
                    conn->stream_error->type = XMPP_SE_REMOTE_CONN_FAILED;
                else if (strcmp(name, "resource-constraint") == 0)
                    conn->stream_error->type = XMPP_SE_RESOURCE_CONSTRAINT;
                else if (strcmp(name, "restricted-xml") == 0)
                    conn->stream_error->type = XMPP_SE_RESTRICTED_XML;
                else if (strcmp(name, "see-other-host") == 0)
                    conn->stream_error->type = XMPP_SE_SEE_OTHER_HOST;
                else if (strcmp(name, "system-shutdown") == 0)
                    conn->stream_error->type = XMPP_SE_SYSTEM_SHUTDOWN;
                else if (strcmp(name, "undefined-condition") == 0)
                    conn->stream_error->type = XMPP_SE_UNDEFINED_CONDITION;
                else if (strcmp(name, "unsupported-encoding") == 0)
                    conn->stream_error->type = XMPP_SE_UNSUPPORTED_ENCODING;
                else if (strcmp(name, "unsupported-stanza-type") == 0)
                    conn->stream_error->type = XMPP_SE_UNSUPPORTED_STANZA_TYPE;
                else if (strcmp(name, "unsupported-version") == 0)
                    conn->stream_error->type = XMPP_SE_UNSUPPORTED_VERSION;
                else if (strcmp(name, "xml-not-well-formed") == 0)
                    conn->stream_error->type = XMPP_SE_XML_NOT_WELL_FORMED;
            }
        } while ((child = xmpp_stanza_get_next(child)));
        
        // 增加引用
        conn->stream_error->stanza = xmpp_stanza_clone(stanza);
    }
    
    return XMPP_HANDLER_AGAIN;;
}

// 处理服务器返回的stream:features
static int _handle_features(xmpp_conn_t *conn, xmpp_stanza_t *stanza, void *userdata)
{
    xmpp_stanza_t *child, *mech;
    char *text;
    
    // 超时计时器关闭
    conn->tls_support = 0;
    conn->sasl_support = 0;
    conn->zlib_support = 0;
    
    // 检查是否启用tls
    if (!conn->secured) {
        if (!conn->tls_disabled) {
            child = xmpp_stanza_get_child_by_name(stanza, "starttls");
            if (child && (strcmp(xmpp_stanza_get_ns(child), XMPP_NS_TLS) == 0))
                conn->tls_support = 1;
        } else {
            // 服务器不支持tls
            conn->tls_support = 0;
        }
    }
    
    // 检查sasl支持
    child = xmpp_stanza_get_child_by_name(stanza, "mechanisms");
    if (child && (strcmp(xmpp_stanza_get_ns(child), XMPP_NS_SASL) == 0)) {
        for (mech = xmpp_stanza_get_children(child); mech;
             mech = xmpp_stanza_get_next(mech)) {
            if (strcmp(xmpp_stanza_get_name_ptr(mech), "mechanism") == 0) {
                text = xmpp_stanza_get_text_ptr(mech);
                if (im_stricmp(text, "PLAIN") == 0)
                    conn->sasl_support |= SASL_MASK_PLAIN;
                else if (im_stricmp(text, "DIGEST-MD5") == 0)
                    conn->sasl_support |= SASL_MASK_DIGESTMD5;
                    
            }
        }
    }
    
    //检查压缩支持
    child = xmpp_stanza_get_child_by_name(stanza, "compression");
    if (child && (strcmp(xmpp_stanza_get_ns(child), XMPP_NS_COMPRESSION) == 0)) {
        for (mech = xmpp_stanza_get_children(child); mech;
             mech = xmpp_stanza_get_next(mech)) {
            if (strcmp(xmpp_stanza_get_name_ptr(mech), "method") == 0) {
                text = xmpp_stanza_get_text_ptr(mech);
                if (im_stricmp(text, "zlib") == 0)
                    conn->zlib_support = 1;
            }
        }
    }
    
    // GO
    _do_auth(conn);
    return XMPP_HANDLER_END;
}

// 接收到proceedtls
static int _handle_proceedtls(xmpp_conn_t *conn, xmpp_stanza_t *stanza, void *userdata)
{
    char *name;
    name = xmpp_stanza_get_name_ptr(stanza);
    xmpp_debug(conn->ctx, "xmpp",
               "handle proceedtls called for %s", name);
               
    if (strcmp(name, "proceed") == 0) {
        xmpp_debug(conn->ctx, "xmpp", "proceeding with TLS");
        // 启动tls握手
        conn_start_ssl(conn);
    }
    
    return XMPP_HANDLER_END;
}

// 接受到SASL结果
static int _handle_sasl_result(xmpp_conn_t *conn, xmpp_stanza_t *stanza, void *userdata)
{
    char *name;
    name = xmpp_stanza_get_name_ptr(stanza);
    
    if (strcmp(name, "failure") == 0) {
        // 服务器返回认证失败
        xmpp_debug(conn->ctx, "xmpp", "SASL %s auth failed", (char*)userdata);
        // 尝试其他可以用的方式(服务器对重试次数可能会有限制，连接可能被关闭)
        _do_auth(conn);
        
    } else if (strcmp(name, "success") == 0) {
        // 服务器返回认证成功！！！
        xmpp_debug(conn->ctx, "xmpp", "SASL %s auth successful", (char *)userdata);
        
        // 再重启stream
        conn_reset_stream(conn, _auth_handle_open_sasl);
        conn_init_stream(conn);
        
    } else {
        xmpp_error(conn->ctx, "xmpp", "Got unexpected reply to SASL %s authentication.",
                   (char *)userdata);
        xmpp_disconnect(conn);
    }
    
    return 0;
}

// 接受服务器给的digestmd5挑战
static int _handle_digestmd5_challenge(xmpp_conn_t *conn, xmpp_stanza_t *stanza,
                                       void *userdata)
{
    char *text;
    char *response;
    xmpp_stanza_t *auth, *authdata;
    char *name;
    
    name = xmpp_stanza_get_name_ptr(stanza);
    xmpp_debug(conn->ctx, "xmpp", "handle digest-md5 (challenge) called for %s", name);
    
    if (strcmp(name, "challenge") == 0) {
        text = xmpp_stanza_get_text_ptr(stanza);
        response = sasl_digest_md5(conn->ctx, text, conn->jid, conn->pass);
        if (!response) {
            disconnect_mem_error(conn);
            return 0;
        }
        
        auth = xmpp_stanza_new(conn->ctx);
        if (!auth) {
            disconnect_mem_error(conn);
            return 0;
        }
        xmpp_stanza_set_name(auth, "response");
        xmpp_stanza_set_ns(auth, XMPP_NS_SASL);
        
        authdata = xmpp_stanza_new(conn->ctx);
        if (!authdata) {
            disconnect_mem_error(conn);
            return 0;
        }
        
        xmpp_stanza_set_text(authdata, response);
        xmpp_free(conn->ctx, response);
        
        xmpp_stanza_add_child(auth, authdata);
        xmpp_stanza_release(authdata);
        
        // 设置响应处理
        handler_add(conn, _handle_digestmd5_rspauth, XMPP_NS_SASL, NULL, NULL, NULL);
        
        xmpp_send(conn, auth);
        xmpp_stanza_release(auth);
        
    } else {
        return _handle_sasl_result(conn, stanza, "DIGEST-MD5");
    }
    
    return XMPP_HANDLER_END;
}

// 接受服务器给的digestmd5挑战结果
static int _handle_digestmd5_rspauth(xmpp_conn_t *conn, xmpp_stanza_t *stanza,
                                     void *userdata)
{
    xmpp_stanza_t *auth;
    char *name;
    
    name = xmpp_stanza_get_name_ptr(stanza);
    xmpp_debug(conn->ctx, "xmpp",
               "handle digest-md5 (rspauth) called for %s", name);
               
               
    if (strcmp(name, "challenge") == 0) {
        /* assume it's an rspauth response */
        auth = xmpp_stanza_new(conn->ctx);
        if (!auth) {
            disconnect_mem_error(conn);
            return 0;
        }
        xmpp_stanza_set_name(auth, "response");
        xmpp_stanza_set_ns(auth, XMPP_NS_SASL);
        xmpp_send(conn, auth);
        xmpp_stanza_release(auth);
    } else {
        return _handle_sasl_result(conn, stanza, "DIGEST-MD5");
    }
    
    return 1;
}

// 接受sasl完成以后的stream
static void _auth_handle_open_sasl(xmpp_conn_t *conn)
{
    xmpp_debug(conn->ctx, "xmpp", "Reopened stream successfully.");
    handler_add(conn, _handle_features_sasl, XMPP_NS_STREAMS, "features", NULL, NULL);
}

// sasl以后要绑定资源或者session
static int _handle_features_sasl(xmpp_conn_t *conn, xmpp_stanza_t *stanza, void *userdata)
{
    xmpp_stanza_t *bind, *session, *iq, *res, *text;
    char *resource;
    
    // 检查服务器是否需要bind
    bind = xmpp_stanza_get_child_by_name(stanza, "bind");
    if (bind && strcmp(xmpp_stanza_get_ns(bind), XMPP_NS_BIND) == 0) {
        conn->bind_required = 1;
    }
    
    // 检查服务器是否需要session
    session = xmpp_stanza_get_child_by_name(stanza, "session");
    if (session && strcmp(xmpp_stanza_get_ns(session), XMPP_NS_SESSION) == 0) {
        conn->session_required = 1;
    }
    
    if (conn->bind_required) {
        // 绑定资源
        handler_add_id(conn, _handle_bind, XMPP_BIND_ID, NULL);
        
        iq = xmpp_stanza_new(conn->ctx);
        if (!iq) {
            disconnect_mem_error(conn);
            return 0;
        }
        
        xmpp_stanza_set_name(iq, "iq");
        xmpp_stanza_set_type(iq, "set");
        xmpp_stanza_set_id(iq, XMPP_BIND_ID);
        
        bind = xmpp_stanza_copy(bind);
        if (!bind) {
            xmpp_stanza_release(iq);
            disconnect_mem_error(conn);
            return 0;
        }
        
        // 要绑定的资源
        resource = xmpp_jid_resource(conn->ctx, conn->jid);
        if ((resource != NULL) && (strlen(resource) == 0)) {
            xmpp_free(conn->ctx, resource);
            resource = NULL;
        }
        
        if (resource) {
            res = xmpp_stanza_new(conn->ctx);
            if (!res) {
                xmpp_stanza_release(bind);
                xmpp_stanza_release(iq);
                disconnect_mem_error(conn);
                return 0;
            }
            xmpp_stanza_set_name(res, "resource");
            text = xmpp_stanza_new(conn->ctx);
            if (!text) {
                xmpp_stanza_release(res);
                xmpp_stanza_release(bind);
                xmpp_stanza_release(iq);
                disconnect_mem_error(conn);
                return 0;
            }
            xmpp_stanza_set_text(text, resource);
            xmpp_stanza_add_child(res, text);
            xmpp_stanza_release(text);
            xmpp_stanza_add_child(bind, res);
            xmpp_stanza_release(res);
            xmpp_free(conn->ctx, resource);
        }
        
        xmpp_stanza_add_child(iq, bind);
        xmpp_stanza_release(bind);
        // 发送
        xmpp_send(conn, iq);
        xmpp_stanza_release(iq);
        
    } else {
        xmpp_error(conn->ctx, "xmpp", "Stream features does not allow "\
                   "resource bind.");
        xmpp_disconnect(conn);
    }
    
    return 0;
}

// 资源绑定结果
static int _handle_bind(xmpp_conn_t *conn, xmpp_stanza_t *stanza, void *userdata)
{
    char *type;
    char *bound_jid;
    xmpp_stanza_t *iq, *session;
    
    type = xmpp_stanza_get_type_ptr(stanza);
    if (type && strcmp(type, "error") == 0) {
        // 绑定失败
        xmpp_error(conn->ctx, "xmpp", "Binding failed.");
        xmpp_disconnect(conn);
        
    } else if (type && strcmp(type, "result") == 0) {
        xmpp_stanza_t *binding = xmpp_stanza_get_child_by_name(stanza, "bind");
        xmpp_debug(conn->ctx, "xmpp", "Bind successful.");
        
        if (binding) {
            xmpp_stanza_t *jid_stanza = xmpp_stanza_get_child_by_name(binding,
                                        "jid");
            if (jid_stanza) {
                bound_jid = xmpp_stanza_get_text_ptr(jid_stanza);
                if (bound_jid) {
                    conn->bound_jid = xmpp_strdup(conn->ctx, bound_jid);
                }
            }
        }
        
        // 是否要建立session
        if (conn->session_required) {
            handler_add_id(conn, _handle_session, XMPP_SESSION_ID, NULL);
            
            iq = xmpp_stanza_new(conn->ctx);
            if (!iq) {
                disconnect_mem_error(conn);
                return 0;
            }
            
            xmpp_stanza_set_name(iq, "iq");
            xmpp_stanza_set_type(iq, "set");
            xmpp_stanza_set_id(iq, XMPP_SESSION_ID);
            
            session = xmpp_stanza_new(conn->ctx);
            if (!session) {
                xmpp_stanza_release(iq);
                disconnect_mem_error(conn);
            }
            
            xmpp_stanza_set_name(session, "session");
            xmpp_stanza_set_ns(session, XMPP_NS_SESSION);
            
            xmpp_stanza_add_child(iq, session);
            xmpp_stanza_release(session);
            
            // 发送
            xmpp_send(conn, iq);
            xmpp_stanza_release(iq);
        } else {
            conn->authenticated = 1;
            
            // 通知外部
            conn->conn_handler(conn, XMPP_CONN_CONNECT, 0, NULL,
                               conn->userdata);
        }
    } else {
        xmpp_error(conn->ctx, "xmpp", "Server sent error bind reply.");
        xmpp_disconnect(conn);
    }
    
    return XMPP_HANDLER_END;
}

static int _handle_session(xmpp_conn_t *conn, xmpp_stanza_t *stanza, void *userdata)
{
    char *type;
    
    type = xmpp_stanza_get_type_ptr(stanza);
    if (type && strcmp(type, "error") == 0) {
        xmpp_error(conn->ctx, "xmpp", "Session establishment failed.");
        xmpp_disconnect(conn);
        
    } else if (type && strcmp(type, "result") == 0) {
        xmpp_debug(conn->ctx, "xmpp", "Session establishment successful.");
        
        // 认证流程成功
        conn->authenticated = 1;
        // 通知外部
        conn->conn_handler(conn, XMPP_CONN_CONNECT, 0, NULL, conn->userdata);
        
    } else {
        xmpp_error(conn->ctx, "xmpp", "Server sent error session reply.");
        xmpp_disconnect(conn);
    }
    
    return 0;
}

/* 执行认证
*/
static void _do_auth(xmpp_conn_t *conn)
{
    xmpp_stanza_t *auth, *authdata;
    char *str, *authid;
    
    if (conn->tls_support && !conn->secured) {
        // 发送starttls
        auth = _make_starttls(conn);
        if (!auth) {
            disconnect_mem_error(conn);
            return;
        }
        // 注册tls proceed处理
        handler_add(conn, _handle_proceedtls, XMPP_NS_TLS, NULL, NULL, NULL);
        
        xmpp_send(conn, auth);
        xmpp_stanza_release(auth);
        
    } else if (!conn->authenticated) {
        // 进行sasl握手流程
        
        if (conn->sasl_support & SASL_MASK_DIGESTMD5) {
            // 尝试DEGESTMD5
            auth = _make_sasl_auth(conn, "DIGEST-MD5");
            if (!auth) {
                disconnect_mem_error(conn);
                return;
            }
            handler_add(conn, _handle_digestmd5_challenge, XMPP_NS_SASL, NULL, NULL, NULL);
            
            xmpp_send(conn, auth);
            xmpp_stanza_release(auth);
            
            // 取消标记
            conn->sasl_support &= ~SASL_MASK_DIGESTMD5;
            
        } else if (conn->sasl_support & SASL_MASK_PLAIN) {
            // 尝试明文认证
            auth = _make_sasl_auth(conn, "PLAIN");
            if (!auth) {
                disconnect_mem_error(conn);
                return;
            }
            authdata = xmpp_stanza_new(conn->ctx);
            if (!authdata) {
                disconnect_mem_error(conn);
                return;
            }
            authid = xmpp_jid_node(conn->ctx, conn->jid);
            if (!authid) {
                disconnect_mem_error(conn);
                return;
            }
            str = sasl_plain(conn->ctx, authid, conn->pass);
            if (!str) {
                disconnect_mem_error(conn);
                return;
            }
            xmpp_stanza_set_text(authdata, str);
            xmpp_free(conn->ctx, str);
            xmpp_free(conn->ctx, authid);
            
            xmpp_stanza_add_child(auth, authdata);
            xmpp_stanza_release(authdata);
            
            handler_add(conn, _handle_sasl_result, XMPP_NS_SASL, NULL, NULL, "PLAIN");
            
            xmpp_send(conn, auth);
            xmpp_stanza_release(auth);
            
            // 取消标记
            conn->sasl_support &= ~SASL_MASK_PLAIN;
            
        } else {
            xmpp_error(conn->ctx, "xmpp", "Server auth error.");
            xmpp_disconnect(conn);
        }
    }
}

void auth_handle_open(xmpp_conn_t *conn)
{
    // 重置所有计时器
    handler_reset_timed(conn, 0);
    // 设置错误处理<error/>函数
    handler_add(conn, _handle_stream_error, XMPP_NS_STREAMS, "error", NULL, NULL);
    // 设置流握手处理<stream:feature/>函数
    handler_add(conn, _handle_features, XMPP_NS_STREAMS, "features", NULL, NULL);
}