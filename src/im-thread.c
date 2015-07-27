#include "im-thread.h"

#include <assert.h>
#include <event2/event.h>
#include <event2/thread.h>
#include <event2/util.h>

#include "list.h"
#include "mm.h"

// 线程本地变量，用于保存struct im_thread指针
#ifdef WIN32
DWORD thread_key_;
#endif
#ifdef POSIX
pthread_key_t thread_key_;
#endif

// 线程消息
typedef struct im_thread_msg {
    int msg_id;
    im_thread_msg_handler handler;
    void *userdata;
    struct event *pos_ev;
    struct list_head msg_node;
} im_thread_msg_t;

// 线程结构
struct im_thread {
    bool wraped;
    bool started;
    void *userdata;
#ifdef POSIX
    pthread_t thread_handle;
    pthread_cond_t signal;
#endif
#ifdef WIN32
    HANDLE thread_handle;
    HANDLE signal;
#endif
    // 每个线程拥有一个eventbase
    struct event_base *base;
    // 待处理的消息列表
    struct list_head msg_head;
    // 线程锁
    im_thread_mutex_t *m_lock;

};

static void _im_thread_msg_free(im_thread_t *t)
{
    im_thread_mutex_lock(t->m_lock);

    struct list_head *pos, *tmp;
    if (!list_empty(&t->msg_head)) {
        list_for_each_safe(pos, tmp, &t->msg_head) {
            im_thread_msg_t *msg = list_entry(pos, im_thread_msg_t, msg_node);
            list_del(pos);
            event_free(msg->pos_ev);
            safe_mem_free(msg);
        }
    }

    im_thread_mutex_unlock(t->m_lock);
}

static void _im_thread_free(im_thread_t *t)
{
    // 释放消息队列
    _im_thread_msg_free(t);

    // 释放event_base
    event_base_free(t->base);

    // 释放信号
#ifdef WIN32
    CloseHandle(t->signal);
#endif
#ifdef POSIX
    pthread_cond_destroy(t->signal);
#endif

    // 释放锁
    im_thread_mutex_destroy(t->m_lock);

    // 释放内存
    safe_mem_free(t);
}

void im_thread_init()
{
    sock_initialize();

    // libevent的线程库也需要初始化
#ifdef WIN32
    evthread_use_windows_threads();
    thread_key_ = TlsAlloc();
#endif
#ifdef POSIX
    evthread_use_pthreads();
    pthread_key_create(&thread_key_, NULL);
#endif
}

void im_thread_destroy()
{
#ifdef WIN32
    TlsFree(thread_key_);
#endif
#ifdef POSIX
    pthread_key_delete(thread_key_);
#endif

    sock_shutdown();
}



im_thread_t *im_thread_new()
{
    im_thread_t *t = safe_mem_calloc(sizeof(im_thread_t), NULL);
    if (t) {
        t->m_lock = im_thread_mutex_create();
        if (!t->m_lock) {
            safe_mem_free(t);
            return NULL;
        }

        int ret = 0;
#ifdef WIN32
        t->signal = CreateEvent(NULL, FALSE, FALSE, NULL);
        if (!t->signal) {
            ret = -1;
        }
#endif
#ifdef POSIX
        ret = pthread_cond_init(t->signal, NULL);
#endif
        if (ret != 0) {
            im_thread_mutex_destroy(t->m_lock);
            safe_mem_free(t);
            return NULL;
        }

        t->base = event_base_new();
        if (t->base == NULL) {
#ifdef WIN32
            CloseHandle(t->signal);
#endif
#ifdef POSIX
            pthread_cond_destroy(t->signal);
#endif
            im_thread_mutex_destroy(t->m_lock);
            safe_mem_free(t);
            return NULL;
        }
        INIT_LIST_HEAD(&t->msg_head);
    }
    return t;
}

void im_thread_break()
{
    // 只能对当前线程调用
    im_thread_t *current = im_thread_current();
    if (current) {
        event_base_loopbreak(current->base);
    }
}

bool im_thread_is_current(im_thread_t *t)
{
    if (t != NULL && im_thread_current() == t) {
        return true;
    }
    return false;
}

void im_thread_stop(im_thread_t *t)
{
    // 不能在当前线程调用
    assert(!im_thread_is_current(t));
    if (t->started && !im_thread_is_current(t)) {
        // 安全拔出
        event_base_loopexit(t->base, NULL);

        // 阻塞等待循环退出
        im_thread_join(t);
    }
}

void im_thread_free(im_thread_t *t)
{
    // 不应该对包装的线程调用free,请调用unwrap.
    assert(!t->wraped);
    // 不应该对当前线程调用
    assert(!im_thread_is_current(t));

    if (!t->wraped && !im_thread_is_current(t)) {
        // 安全停止
        im_thread_stop(t);
        // 释放
        _im_thread_free(t);
    }
}

im_thread_t *im_thread_current()
{
#ifdef WIN32
    return (im_thread_t*)(TlsGetValue(thread_key_));
#endif
#ifdef POSIX
    return (im_thread_t*)(pthread_getspecific(thread_key_));
#endif
}

static void _im_thread_set_current(im_thread_t *t)
{
#ifdef WIN32
    TlsSetValue(thread_key_, t);
#endif
#ifdef POSIX
    pthread_setspecific(key_, thread);
#endif
}

im_thread_t *im_thread_wrap_current()
{
    im_thread_t *current = im_thread_current();
    if (!current) {
        current = im_thread_new();
        if (current) {
#ifdef WIN32
            current->thread_handle = OpenThread(SYNCHRONIZE, FALSE, GetCurrentThreadId());
#endif
#ifdef POSIX
            current->thread_handle = pthread_self();
#endif
            if (!current->thread_handle) {
                im_thread_free(current);
                return NULL;
            }
            current->wraped = true;
            current->started = true;
            _im_thread_set_current(current);
        }
    }
    return current;
}

void im_thread_unwrap_current()
{
    im_thread_t *current = im_thread_current();
    if (current && current->wraped) {
        _im_thread_set_current(NULL);

        im_thread_free(current);
    }
}

void im_thread_join(im_thread_t *t)
{
    // 不允许在当前线程调用当前线程的join
    assert(!im_thread_is_current(t));
    if (t->started && !im_thread_is_current(t)) {

#ifdef WIN32
        WaitForSingleObject(t->thread_handle, INFINITE);
        CloseHandle(t->thread_handle);
        t->thread_handle = NULL;
#endif
#ifdef POSIX
        void *pv;
        pthread_join(thread_, &pv);
#endif
        t->started = false;
    }
}

static void _im_thread_loop(im_thread_t *t)
{
    event_base_dispatch(t->base);
}

static void *_im_thread_runnable_proxy(im_thread_t *running)
{
    _im_thread_set_current(running);
    _im_thread_loop(running);

    // 这里是为了兼容pthread的回调原型
    return NULL;
}

bool im_thread_start(im_thread_t *t, void *userdata)
{
    // 进入共享区域
    im_thread_mutex_lock(t->m_lock);

    // 不能对当前线程或者已经启动的线程调用start
    assert(!t->started);
    assert(!im_thread_is_current(t));

    if (t->started || im_thread_is_current(t))
        return false;

    t->userdata = userdata;
#ifdef WIN32
    // 默认堆栈大小
    t->thread_handle =
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)_im_thread_runnable_proxy, t, 0, NULL);

    if (t->thread_handle != NULL) {
        t->started = true;
    }
#endif
#ifdef POSIX
    int error_code = pthread_create(&thread_handle, NULL, _im_thread_runnable_proxy, t);
    if (!error_code) {
        t->started = true;
    }
#endif
    // 退出共享区域
    im_thread_mutex_unlock(t->m_lock);
    return t->started;
}

int im_thread_run(void *userdata)
{
    im_thread_t *current = im_thread_current();
    if (current && current->wraped) {
        _im_thread_loop(current);
        return 0;
    }
    return -1;
}

static void _im_thread_post_proxy(evutil_socket_t fd, short what, void *arg)
{
    im_thread_msg_t *msg = arg;
    msg->handler(msg->msg_id, msg->userdata);
    im_thread_t *current = im_thread_current();

    // 线程安全
    im_thread_mutex_lock(current->m_lock);
    list_del(&msg->msg_node);
    im_thread_mutex_unlock(current->m_lock);

    event_free(msg->pos_ev);
    safe_mem_free(msg);
}

void im_thread_post(im_thread_t *sink, int msg_id, im_thread_msg_handler handler, long milliseconds,
                    void *userdata)
{
    if (!sink && !(sink = im_thread_current())) {
        return;
    }
    im_thread_msg_t *msg = safe_mem_malloc(sizeof(im_thread_msg_t), NULL);
    if (msg) {
        struct event *pos_ev = event_new(sink->base, -1, EV_TIMEOUT, _im_thread_post_proxy, msg);
        if (!pos_ev) {
            safe_mem_free(msg);
            return;
        }
        msg->handler = handler;
        msg->msg_id = msg_id;
        msg->userdata = userdata;
        msg->pos_ev = pos_ev;

        // 线程安全
        im_thread_mutex_lock(sink->m_lock);
        list_add(&msg->msg_node, &sink->msg_head);
        im_thread_mutex_unlock(sink->m_lock);

        struct timeval ts;
        ts.tv_sec = milliseconds / 1000;
        ts.tv_usec = (milliseconds % 1000) * 1000;

        event_add(pos_ev, &ts);
    }
}

static void _im_thread_send_proxy(evutil_socket_t fd, short what, void *arg)
{
    im_thread_msg_t *msg = arg;
    msg->handler(msg->msg_id, msg->userdata);
    im_thread_t *current = im_thread_current();
#ifdef WIN32
    SetEvent(current->signal);
#endif
#ifdef POSIX
    pthread_cond_signal(current->signal);
#endif
    // 线程安全只保证消息队列的添加删除是安全的
    im_thread_mutex_lock(current->m_lock);
    list_del(&msg->msg_node);
    im_thread_mutex_unlock(current->m_lock);

    event_free(msg->pos_ev);
    safe_mem_free(msg);
}

void im_thread_send(im_thread_t *sink, int msg_id, im_thread_msg_handler handler, void *userdata)
{
    if (!sink && !(sink = im_thread_current())) {
        return;
    }
    if (sink == im_thread_current()) {
        handler(msg_id, userdata);
    } else {
        im_thread_msg_t *msg = safe_mem_malloc(sizeof(im_thread_msg_t), NULL);
        if (msg) {
            struct event *pos_ev = event_new(sink->base, -1, 0, _im_thread_send_proxy, msg);
            if (!pos_ev) {
                safe_mem_free(msg);
                return;
            }
            msg->handler = handler;
            msg->msg_id = msg_id;
            msg->userdata = userdata;
            msg->pos_ev = pos_ev;

            // 线程安全只保证消息队列的添加删除是安全的
            im_thread_mutex_lock(sink->m_lock);
            list_add(&msg->msg_node, &sink->msg_head);
            im_thread_mutex_unlock(sink->m_lock);

            struct timeval ts;
            ts.tv_sec = 0;
            ts.tv_usec = 0;

            event_add(pos_ev, &ts);
#ifdef WIN32
            WaitForSingleObject(sink->signal, INFINITE);
#endif
#ifdef POSIX
            pthread_cond_wait(sink->signal, sink->m_lock);
#endif
        }
    }
}

struct event_base *im_thread_get_eventbase(im_thread_t *t)
{
    im_thread_t *current = t ? t : im_thread_current();
    if (current) {
        return current->base;
    }
    return NULL;
}

void *im_thread_get_userdata(im_thread_t *t)
{
    im_thread_t *current = t ? t : im_thread_current();
    if (current) {
        return current->userdata;
    }
    return NULL;
}

struct im_thread_mutex {
#ifdef WIN32
    HANDLE mutex_handler;
#endif
#ifdef POSIX
    pthread_mutex_t *mutex_handler;
#endif
};

im_thread_mutex_t * im_thread_mutex_create()
{
    im_thread_mutex_t *mutex;

    mutex = (im_thread_mutex_t*)safe_mem_malloc(sizeof(im_thread_mutex_t), NULL);

    if (mutex) {
#ifdef WIN32
        mutex->mutex_handler = CreateMutex(NULL, FALSE, NULL);
#endif
#ifdef POSIX
        mutex->mutex_handler = safe_mem_malloc(sizeof(pthread_mutex_t), NULL);
        if (mutex->mutex_handler) {
            if (pthread_mutex_init(mutex->mutex_handler, NULL) != 0) {
                safe_mem_free(mutex->mutex_handler);
                mutex->mutex_handler = NULL;
            }
        }
#endif
        if (!mutex->mutex_handler) {
            safe_mem_free(mutex);
            mutex = NULL;
        }
    }

    return mutex;
}

int im_thread_mutex_destroy(im_thread_mutex_t *mutex)
{
    int ret = 1;
#ifdef WIN32
    if (mutex->mutex_handler)
        ret = CloseHandle(mutex->mutex_handler);
#endif
#ifdef POSIX
    if (mutex->mutex_handler)
        ret = pthread_mutex_destroy(mutex->mutex_handler) == 0;
#endif
    safe_mem_free(mutex);
    return ret;
}

int im_thread_mutex_lock(im_thread_mutex_t *mutex)
{
    int ret = 1;
#ifdef WIN32
    ret = WaitForSingleObject(mutex->mutex_handler, INFINITE) == 0;
#endif
#ifdef POSIX
    ret = pthread_mutex_lock(mutex->mutex_handler) == 0;
#endif
    return ret;
}

int im_thread_mutex_unlock(im_thread_mutex_t *mutex)
{
    int ret = 1;
#ifdef WIN32
    ret = ReleaseMutex(mutex->mutex_handler);
#endif
#ifdef POSIX
    ret = pthread_mutex_unlock(mutex->mutex_handler) == 0;
#endif
    return ret;
}

bool im_thread_sleep(int milliseconds)
{
#ifdef WIN32
    Sleep(milliseconds);
    return true;
#endif
#ifdef POSIX
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    int ret = nanosleep(&ts, NULL);
    if (ret != 0) {
        return false;
    }
    return true;
#endif
}
