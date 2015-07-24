// imcore.h 即时通讯接口文件
//
#ifndef __IMCORE_H__
#define __IMCORE_H__

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef IMCORE_EXPORTS
#define IMCORE_API __declspec(dllexport)
#else
#define IMCORE_API __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    IM_RESULT_OK = 0,               // 执行没问题
    IM_RESULT_ERROR,                // 通用错误
    IM_RESULT_BADARGUMENT,          // 参数错误
    IM_RESULT_BADSTATE,             // 错误状态
    IM_RESULT_PENDING,              // 处理中
    IM_RESULT_TIMEOUT,              // 超时
    IM_RESULT_NOTYETIMPLEMENTED,    // 功能还没有实现
} im_result;

typedef enum {
    IM_STATE_INIT,                  // 最开始创建以后的状态
    IM_STATE_OPENING,               // 正在连接状态
    IM_STATE_OPEN,                  // 连接成功状态
    IM_STATE_CLOSED,                // 连接关闭
} im_state;


// 初始化库
IMCORE_API
bool im_init();
IMCORE_API
void im_destroy();


// 会话句柄
typedef struct im_session im_session_t;

// IM消息句柄
typedef struct im_msg im_msg_t;

// IM会话状态回调函数签名
typedef void(*im_state_handler)(im_session_t *session, im_state state, void *userdate);

// IM文本消息回调函数签名
typedef void(*im_msg_handler)(im_session_t *session, im_msg_t *msg, void *userdate);

// 新建一个会话
IMCORE_API
im_session_t *im_session_new(const char *host, const char *username, const char *password,
                             im_state_handler state_handler, im_msg_handler msg_handler, void *userdate);

// 发起网络连接
IMCORE_API
im_result im_session_open(im_session_t *session);

// 关闭
IMCORE_API
im_result im_session_close(im_session_t *session);

// 创建一个消息
IMCORE_API
im_msg_t *im_msg_new(im_session_t *session, const char *to);

// 释放一个消息
IMCORE_API
void im_msg_free(im_msg_t *media_msg);

// 消息属性
//
IMCORE_API
uint64_t im_msg_get_id(im_msg_t *media_msg);
IMCORE_API
char *im_msg_get_from(im_msg_t *media_msg);
IMCORE_API
char *im_msg_get_to(im_msg_t *media_msg);
IMCORE_API
char *im_msg_get_type(im_msg_t *media_msg);

// 发送消息
IMCORE_API
im_result im_msg_send(im_msg_t *im_msg, bool offline);


/* 消息扩展 */

// 下载媒体文件回调
typedef void(*im_media_fetch_cb)(im_msg_t *msg, im_result result, void *userdate);

// 读一条文本消息， 返回接收文本的长度 -1表示错误
IMCORE_API
int im_text_msg_read(const im_msg_t *im_msg, char *buff, size_t len);

// 写入一条文本消息,  写入的长如 -1表示错误
IMCORE_API
int im_text_msg_write(im_msg_t *im_msg, const char *buff, size_t len);


// 图片尺寸
typedef enum {
    IM_IMG_SMALL,
    IM_IMG_NORMAL
} im_img_profile;

// 图片类型 （以后提供转码功能，当前作为客户端之间的协商标识）
typedef enum {
    IM_IMG_BMP,
    IM_IMG_PNG,
    IM_IMG_JPG,
    IM_IMG_GIF
} im_img_type;

// 读取图片消息信息, 成功返回true， 作为该图片的句柄
IMCORE_API
bool im_image_msg_read(const im_msg_t *im_msg, im_img_type *img_type,
                       const char *name, size_t *type_len, uint64_t *img_size);

// 获取图片
IMCORE_API
bool im_image_msg_fetch(im_msg_t *im_msg, const char *path, const char *name, im_img_type img_type,
                        im_img_profile img_profile, im_media_fetch_cb fetch_cb, void *userdata);

// 附加一个图片给消息
IMCORE_API
bool im_image_msg_add(im_msg_t *im_msg, const char *path, const char *name, im_img_type img_type,
                      im_media_fetch_cb sent_cb, void *userdata);

// 声音编码类型（以后提供转码功能，当前作为客户端之间的协商标识）
typedef enum {
    IM_VOICE_ENCODE_VORBIS,
    IM_VOICE_ENCODE_OPUS,
    IM_VOICE_ENCODE_PCM,
    IM_VOICE_ENCODE_MP3,
    IM_VOICE_ENCODE_ACCLC,
} im_voice_codec;

// 声音文件容器类型
typedef enum {
    IM_VOICE_CONTAINER_MP3,
    IM_VOICE_CONTAINER_MP4,
    IM_VOICE_CONTAINER_WAVE,
    IM_VOICE_CONTAINER_3GP,
    IM_VOICE_CONTAINER_OGG
} im_voice_container;

// 读取音频消息
IMCORE_API
bool im_voice_msg_read(const im_msg_t *im_msg, im_voice_codec *codec, im_voice_container *container,
                       size_t *file_size, time_t *voice_len);

// 获取声音文件.
IMCORE_API
bool im_voice_msg_fetch(im_msg_t *im_msg, const char *path, const char *name, im_voice_codec codec,
                        im_voice_container container, im_media_fetch_cb fetch_cb, void *userdata);

// 添加一个音频文件到消息
IMCORE_API
bool im_voice_msg_add(im_msg_t *im_msg, const char *path, const char *name, im_voice_codec codec,
                      im_voice_container container, im_media_fetch_cb sent_cb, void *userdata);

// 呼叫状态
typedef enum {
    IM_CALL_CALLING,
    TM_CALL_ACCEPTED,
    IM_CALL_REJECTED,
    IM_CALL_CONNECTED,
    IM_CALL_HANGUP
} im_call_state;

// 呼叫会话
typedef struct im_call_session im_call_session_t;

// 数据类型是input_codec编码后的数据
typedef void(*im_call_data_cb)(im_call_session_t *call_session, const char *buf, size_t len,
                               void *userdata);

// 呼叫状态改变
typedef void(*im_call_state_cb)(im_call_session_t *call_session, im_call_state state,
                                void *userdata);

// 创建一个呼叫请求
IMCORE_API
bool im_call_msg_open(im_msg_t *im_msg, im_voice_codec output_codec, im_voice_codec accept_codec,
                      im_call_state_cb state_cb,
                      im_call_data_cb recv_cb);

// 读取呼叫消息codec默认都是PCM, 原因是方便播放以及录制(底层传输用的编码为Opus)
IMCORE_API
bool im_call_msg_read(const im_msg_t *im_msg, im_voice_codec *output_codec,
                      im_voice_codec *input_codec);

// 开启一个呼叫session
IMCORE_API
im_call_session_t *im_call_open(im_msg_t *im_msg, im_voice_codec output_codec,
                                im_voice_codec accept_codec,
                                im_call_state_cb state_cb, im_call_data_cb recv_cb);

// 关闭呼叫session
IMCORE_API
void im_call_close(im_call_session_t *call_session);

// 同意或者拒绝呼叫请求
IMCORE_API
void im_call_accept(im_msg_t *im_msg, im_call_session_t *call_session, bool accept,
                    const char *reason,
                    im_call_state_cb state_cb, im_call_data_cb recv_cb, void *userdata);

// 挂断
IMCORE_API
void im_call_hangup(im_call_session_t *call_session);

// 数据类型必须是output_codec编码的数据, 返回发送的数据长度
IMCORE_API
int im_call_send_data(im_call_session_t *call_session, char *buf, size_t len);


#ifdef __cplusplus
}
#endif

#endif // __IMCORE_H__