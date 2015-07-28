#include "im-inl.h"

static void _xmpp_conn_handler(xmpp_conn_t *conn, xmpp_conn_event_t status,
                               const int error, xmpp_stream_error_t *stream_error,
                               void *userdata);

static void _im_conn_pre_free(im_conn_t *conn);



im_conn_t *im_conn_new(const char *host,
                       const char *username,
                       const char *password,
                       im_conn_state_cb statecb,
                       im_conn_recive_cb msgcb,
                       void *userdate)
{
    im_conn_t *conn = NULL;

    assert(username && password && statecb && msgcb);
    if (!username || !password || !statecb || !msgcb)
        return NULL;


    bool done = false;
    // ��ʼ�����ڴ�
    do {
        conn = safe_mem_calloc(sizeof(im_conn_t), NULL);
        if (!conn)
            break;

        conn->signal_thread = im_thread_new();
        conn->work_thread = im_thread_new();
        if (!conn->signal_thread &&  !conn->work_thread)
            break;

        // xmpp���Ĺ������ź��߳�
        conn->xmpp_ctx = xmpp_ctx_new(conn->signal_thread, NULL);
        if (!conn->xmpp_ctx)
            break;

        conn->xmpp_conn = xmpp_conn_new(conn->xmpp_ctx);
        if (!conn->xmpp_conn)
            break;

        if (host) {
            conn->xmpp_host = im_strndup(host, 256);
            if (!conn->xmpp_host)
                break;
        }

        done = true;
    } while (0);

    // �ڴ����ʧ��
    if (!done && conn) {
        _im_conn_pre_free(conn);
        return NULL;
    }

    // �ڴ����ɹ�, ��ʼ��ֵ
    conn->statecb = statecb;
    conn->msgcb = msgcb;
    conn->userdata = userdate;

    // ����xmpp����
    xmpp_conn_t *xmpp_conn = conn->xmpp_conn;
    xmpp_conn_set_jid(xmpp_conn, username);
    xmpp_conn_set_pass(xmpp_conn, password);

    conn->state = IM_STATE_INIT;
    return conn;
}

IMCORE_API int im_conn_open(im_conn_t *conn)
{
    if (conn->state != IM_STATE_INIT) {
        return -1;
    }

    xmpp_connect_client(conn->xmpp_conn, conn->xmpp_host, 5222,
                        _xmpp_conn_handler, conn);

    // �����ź��߳�
    im_thread_start(conn->signal_thread, conn);
    // ���������߳�
    im_thread_start(conn->work_thread, conn);
    return 0;
}

static void _im_conn_pre_free(im_conn_t *conn)
{

}

static void _xmpp_conn_handler(xmpp_conn_t *xmpp_conn, xmpp_conn_event_t status,
                               const int error, xmpp_stream_error_t *stream_error,
                               void *userdata)
{
    im_conn_t *conn = (im_conn_t*)userdata;

    if (status == XMPP_CONN_CONNECT) {
        // ע����Ϣ<message></message>������
        xmpp_handler_add(xmpp_conn, msg_text_handler, NULL, "message", NULL, conn);
        // ע��file��Ϣ������
        xmpp_id_handler_add(xmpp_conn, msg_file_handler, IM_MSG_FILE_ID, conn);
        // ֪ͨ���ӳɹ�
        conn->statecb(conn, IM_STATE_OPEN, 0, conn->userdata);
    } else if (status == XMPP_CONN_DISCONNECT) {
        conn->statecb(conn, IM_STATE_CLOSED, 0, conn->userdata);
    } else if (status == XMPP_CONN_FAIL) {
        conn->statecb(conn, IM_STATE_CLOSED, -1, conn->userdata);
    }
}
