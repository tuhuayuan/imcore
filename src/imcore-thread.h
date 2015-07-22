/*
* imcore-thread.h
* 线程模块
*/
#ifndef _IMCORE_THREAD_H
#define _IMCORE_THREAD_H

#include <stdint.h>
#include <stdbool.h>

#ifdef POSIX
#include <pthread.h>
#endif

#ifdef WIN32
#include <winsock2.h>
#include <windows.h>
#endif

typedef struct im_thread im_thread_t;

// 线程库
void im_thread_init();
void im_thread_destroy();

// 创建销毁
im_thread_t *im_thread_new();
im_thread_t *im_thread_wrap_current();
void im_thread_unwrap_current();
void im_thread_free(im_thread_t *t);
im_thread_t *im_thread_current();

// Runnable
typedef void(*im_thread_runnable)(void *userdata);

// 运行控制
void im_thread_quit(im_thread_t *t);
bool im_thread_is_quit(im_thread_t *t);
// 不能对当前线程调用,让当前线程停止请调用im_thread_quit
void im_thread_stop(im_thread_t *t);
bool im_thread_start(im_thread_t *t, im_thread_runnable runnable, void *userdata);
void im_thread_run(im_thread_t *t);
void im_thread_join(im_thread_t *t);
bool im_thread_is_current(im_thread_t *t);

// 当前执行线程休眠
bool im_thread_sleep(int milliseconds);

// Message callback
typedef void(*im_thread_cb)(int msg_id, void *userdata);

// 消息发送
void im_thread_post(im_thread_t *sink, int msg_id, im_thread_cb handler, long milliseconds,
                    void *userdata);
void im_thread_send(im_thread_t *sink, int msg_id, im_thread_cb handler, void *userdata);

// 获取线程的evenbase，后续可以加入一些libevent的用法的包装
// 当前仅暴露一个原始的event_base指针
struct event_base *im_thread_get_eventbase(im_thread_t *t);

// 线程互斥锁
typedef struct im_thread_mutex im_thread_mutex_t;

im_thread_mutex_t *im_thread_mutex_create();
int im_thread_mutex_destroy(im_thread_mutex_t *mutex);
int im_thread_mutex_lock(im_thread_mutex_t *mutex);
int im_thread_mutex_unlock(im_thread_mutex_t *mutex);


#endif // _IMCORE_THREAD_H