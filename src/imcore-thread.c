#include "imcore-thread.h"

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
    im_thread_cb handler;
    void *userdata;
    struct event *pos_ev;
    struct list_head msg_node;
} im_thread_msg_t;

// 线程结构
struct im_thread {
    bool wraped;
    bool started;
    bool release_on_end;
    bool loop_stop;
    im_thread_runnable runnable;
    void *userdata;
#ifdef POSIX
    pthread_t thread_handle;
#endif
#ifdef WIN32
    HANDLE thread_handle;
#endif
    // 每个线程拥有一个eventbase
    struct event_base *base;
    // 待处理的消息列表
    struct list_head msg_head;
    // 线程锁
    im_thread_mutex_t *mutex;
};

void im_thread_init()
{
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
}

static void _im_thread_msg_free(im_thread_t *t)
{
    struct list_head *pos, *tmp;
    if (!list_empty(&t->msg_head)) {
        list_for_each_safe(pos, tmp, &t->msg_head) {
            im_thread_msg_t *msg = list_entry(pos, im_thread_msg_t, msg_node);
            list_del(pos);
            event_free(msg->pos_ev);
            safe_mem_free(msg);
        }
    }
}

im_thread_t *im_thread_new()
{
    im_thread_t *t = safe_mem_calloc(sizeof(im_thread_t), NULL);
    if (t) {
        t->mutex = im_thread_mutex_create();
        if (!t->mutex) {
            safe_mem_free(t);
            return NULL;
        }
        t->base = event_base_new();
        if (t->base == NULL) {
            safe_mem_free(t);
            im_thread_mutex_destroy(t->mutex);
            return NULL;
        }
        INIT_LIST_HEAD(&t->msg_head);
    }
    return t;
}

void im_thread_quit(im_thread_t *t)
{
    t->loop_stop = true;
}

bool im_thread_is_quit(im_thread_t *t)
{
    return t->loop_stop;
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
    im_thread_quit(t);
    im_thread_join(t);
}

void im_thread_free(im_thread_t *t)
{
    if (!t->wraped) {
        im_thread_stop(t);
    }

    _im_thread_msg_free(t);
    event_base_free(t->base);
    im_thread_mutex_destroy(t->mutex);
    safe_mem_free(t);
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
    if (t->started) {
        assert(!im_thread_is_current(t));

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
    while(!t->loop_stop) {
        event_base_loop(t->base, EVLOOP_ONCE);
    }
}

static void *_im_thread_runnable_proxy(im_thread_t *running)
{
    _im_thread_set_current(running);
    if (running->runnable) {
        running->runnable(running->userdata);
    } else {
        _im_thread_loop(running);
    }
    return NULL;
}

bool im_thread_start(im_thread_t *t, im_thread_runnable runnable, void *userdata)
{
    if (t->started || t->wraped)
        return false;

    t->userdata = userdata;
    t->runnable = runnable;

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
    return t->started;
}

void im_thread_run(im_thread_t *t)
{
    if (t->wraped) {
        _im_thread_loop(t);
    } else {
        im_thread_start(t, NULL, NULL);
        im_thread_join(t);
    }
}

static void _im_thread_post_proxy(evutil_socket_t fd, short what, void *arg)
{
    im_thread_msg_t *msg = arg;
    msg->handler(msg->msg_id, msg->userdata);

    im_thread_t *current = im_thread_current();

    im_thread_mutex_lock(current->mutex);
    list_del(&msg->msg_node);
    im_thread_mutex_unlock(current->mutex);

    event_free(msg->pos_ev);
    safe_mem_free(msg);
}

void im_thread_post(im_thread_t *sink, int msg_id, im_thread_cb handler, long milliseconds,
                    void *userdata)
{
    if (!sink && !(sink = im_thread_current())) {
        return;
    }
    im_thread_msg_t *msg = safe_mem_malloc(sizeof(im_thread_msg_t), NULL);
    if (msg) {
        struct event *pos_ev = event_new(sink->base, -1, 0, _im_thread_post_proxy, msg);
        if (!pos_ev) {
            safe_mem_free(msg);
            return;
        }
        msg->handler = handler;
        msg->msg_id = msg_id;
        msg->userdata = userdata;
        msg->pos_ev = pos_ev;

        im_thread_mutex_lock(sink->mutex);
        list_add(&msg->msg_node, &sink->msg_head);
        im_thread_mutex_unlock(sink->mutex);

        struct timeval ts;
        ts.tv_sec = milliseconds / 1000;
        ts.tv_usec = (milliseconds % 1000) * 1000000;
        event_add(pos_ev, &ts);
    }
}

void im_thread_send(im_thread_t *sink, int msg_id, im_thread_cb handler, void *userdata)
{

}

struct event_base *im_thread_get_eventbase(im_thread_t *t)
{
    return t->base;
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
