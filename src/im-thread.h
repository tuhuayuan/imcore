/*
* imcore-thread.h
* 线程模块
*/
#ifndef _IMCORE_THREAD_H
#define _IMCORE_THREAD_H

#include <stdint.h>
#include <stdbool.h>

#include "sock.h"

#ifdef POSIX
#include <pthread.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief IM线程指针
 */
typedef struct im_thread im_thread_t;

/**
 * @brief 初始化线程库。
 * @details 初始化libevent线程库，以及全局的线程本地变量。
 */
void im_thread_init();

/**
 * @brief 释放线程库
 * @details 释放线程本地变量
 */
void im_thread_destroy();

/**
 * @brief 创建一个新的线程对象。
 * @details 一个IM线程包含自己的消息循环，可以使用post，send等方法
 * 把操作交给线程去做。
 * @return im_thread_t* 或者 NULL
 */
im_thread_t *im_thread_new();

/**
 * @brief 包装当前系统线程
 * @details 创建一个新的IM线程，使用当前系统线程。break，stop等线程循环控制
 * 函数不能对包装的线程调用，释放必须调用unwrap
 *
 * @return im_thread_t* 或者 NULL
 */
im_thread_t *im_thread_wrap_current();

/**
 * @brief 释放包装线程
 * @details 该方法只能对当前线程是包装线程的情况下调用，为了保证不勿用所以给没有提供输入参数.
 * 通常在系统线程中按如下方式调用
 * wrap -> run (阻塞) -> unwrap
 */
void im_thread_unwrap_current();

/**
 * @brief 释放IM线程
 * @details 如果线程没有停止, 函数会阻塞等待线程结束后释放
 *
 * @param t 需要释放的线程指针
 */
void im_thread_free(im_thread_t *t);

/**
 * @brief 获取当前线程指针
 *
 * @return im_thread_t* 或者 NULL 表示当前线程不是一个IM线程
 */
im_thread_t *im_thread_current();

/**
 * @brief 给定线程是否是当前线程
 *
 * @param t 需要判别的线程指针
 * @return true 或者 false t为NULL或者当前线程不是IM或者t不是当前线程
 */
bool im_thread_is_current(im_thread_t *t);

/**
 * @brief 强制结束线程
 * @details 该方法作用于当前IM线程, 在当前消息处理完以后立即结束循环
 * 会导致run(包装的线程 ), join, stop的阻塞立即结束.
 */
void im_thread_break();

/**
 * @brief 请求线程结束
 * @details 向线程发送结束请求, 并且等待线程安全结束, 所有当前激活的消息都会被执行.
 * 由于不能在当前线程等待. 对当前线程结束, 因为这将是一个死循环.
 *
 * @param t 请求结束的线程指针
 */
void im_thread_stop(im_thread_t *t);

/**
 * @brief 启动线程
 * @details 启动一个IM线程有并且立即返回,不能对当前线程调用
 *
 * @param t 要启动的线程
 * @param userdata 用户自定义数据
 *
 * @return true 表示线程已经启动
 */
bool im_thread_start(im_thread_t *t, void *userdata);

/**
 * @brief 启动消息队列, 阻塞直到退出消息循环
 * @details 没有给定线程指针,所以只能对当前线程调用. 线程没启动前获取不到当前线程
 * 因此这个API设计的目的是用来启动wrap线程的.
 *
 * @param userdata 用户自定义数据
 * @return 0 表示运行成功
 */
int im_thread_run(void *userdata);

/**
 * @brief 阻塞当前线程等待给定线程结束
 * @details 不能再当前线程等待自己退出, 会导致死循环.
 *
 * @param t 要等待的线程
 */
void im_thread_join(im_thread_t *t);

/**
 * @brief 当前线程挂起
 *
 * @param milliseconds 毫秒
 * @return true 表示成功休眠
 */
bool im_thread_sleep(int milliseconds);

/**
 * @brief 消息处理函数
 *
 * @param b 消息id
 * @param userdata 自定义数据
 *
 */
typedef void(*im_thread_msg_handler)(int msg_id, void *userdata);

/**
 * @brief POST消息给指定线程
 * @details 等同于window的post消息行为, 可以给指定一个延时.
 *
 * @param sink 处理线程
 * @param msg_id 消息id
 * @param handler 消息处理函数
 * @param milliseconds 延时
 * @param userdata 自定义数据
 */
void im_thread_post(im_thread_t *sink, int msg_id, im_thread_msg_handler handler, long milliseconds,
                    void *userdata);

/**
 * @brief SEND一个消息给指定线程
 * @details 等同于window的send消息, 调用会阻塞到指定线程执行完消息处理函数返回
 *
 * @param sink 处理线程
 * @param msg_id 消息id
 * @param handler 消息处理函数
 * @param userdata 自定义数据
 */
void im_thread_send(im_thread_t *sink, int msg_id, im_thread_msg_handler handler, void *userdata);

/**
 * @brief 获取线程的even_base
 *
 * @param t 线程指针或者NULL
 * @return event_base* 或者 NULL
 */
struct event_base *im_thread_get_eventbase(im_thread_t *t);

/**
 * @brief 获取线程的自定义数据
 *
 * @param t 线程指针或者NULL
 * @return 或者 NULL
 */
void *im_thread_get_userdata(im_thread_t *t);

/**
 * @brief 相当于window的Mutex
 */
typedef struct im_thread_mutex im_thread_mutex_t;

/**
 * @brief 创建一个mutex
 * @return NULL 或者 im_thread_mutex_t*
 */
im_thread_mutex_t *im_thread_mutex_create();

/**
 * @brief 销毁一个mutex
 * @param mutex
 * @return 0 表示正常
 */
int im_thread_mutex_destroy(im_thread_mutex_t *mutex);

/**
 * @brief 获取锁
 * @details 获取成功则返回并且独占锁, 或者一个等待
 *
 * @param mutex
 * @return 0 表示正常
 */
int im_thread_mutex_lock(im_thread_mutex_t *mutex);

/**
 * @brief 释放锁
 *
 * @param mutex
 * @return 0 表示正常
 */
int im_thread_mutex_unlock(im_thread_mutex_t *mutex);

#ifdef __cplusplus
}
#endif


#endif // _IMCORE_THREAD_H