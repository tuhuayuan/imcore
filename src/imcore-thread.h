/*
* imcore-thread.h
* �߳�ģ��
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

// �߳̿�
void im_thread_init();
void im_thread_destroy();

// ��������
im_thread_t *im_thread_new();
im_thread_t *im_thread_wrap_current();
void im_thread_unwrap_current();
void im_thread_free(im_thread_t *t);
im_thread_t *im_thread_current();

// Runnable
typedef void(*im_thread_runnable)(void *userdata);

// ���п���
void im_thread_quit(im_thread_t *t);
bool im_thread_is_quit(im_thread_t *t);
// ���ܶԵ�ǰ�̵߳���,�õ�ǰ�߳�ֹͣ�����im_thread_quit
void im_thread_stop(im_thread_t *t);
bool im_thread_start(im_thread_t *t, im_thread_runnable runnable, void *userdata);
void im_thread_run(im_thread_t *t);
void im_thread_join(im_thread_t *t);
bool im_thread_is_current(im_thread_t *t);

// ��ǰִ���߳�����
bool im_thread_sleep(int milliseconds);

// Message callback
typedef void(*im_thread_cb)(int msg_id, void *userdata);

// ��Ϣ����
void im_thread_post(im_thread_t *sink, int msg_id, im_thread_cb handler, long milliseconds,
                    void *userdata);
void im_thread_send(im_thread_t *sink, int msg_id, im_thread_cb handler, void *userdata);

// ��ȡ�̵߳�evenbase���������Լ���һЩlibevent���÷��İ�װ
// ��ǰ����¶һ��ԭʼ��event_baseָ��
struct event_base *im_thread_get_eventbase(im_thread_t *t);

// �̻߳�����
typedef struct im_thread_mutex im_thread_mutex_t;

im_thread_mutex_t *im_thread_mutex_create();
int im_thread_mutex_destroy(im_thread_mutex_t *mutex);
int im_thread_mutex_lock(im_thread_mutex_t *mutex);
int im_thread_mutex_unlock(im_thread_mutex_t *mutex);


#endif // _IMCORE_THREAD_H