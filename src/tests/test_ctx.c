#include <stdio.h>
#include <stdbool.h>

#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

#include "xmpp.h"
#include "mm.h"
#include "pthread-win32.h"
#include "imcore-thread.h"

#include "tests/test_message.h"
#include "tests/test_thread.h"

pthread_t console_thread;
pthread_t xmpp_thread;

char *require_string(const char *promote, size_t len, const char* opt)
{
    char *required = NULL;
    char *read = NULL;
    size_t read_len = 0;
    
    printf(promote);
    if (opt != NULL) {
        printf("(%s)", opt);
    }
    printf(": ");
    
    required = (char*)safe_mem_malloc(sizeof(char)*(len+2), NULL);   // 一个换行一个0
    if (!required)
        return NULL;
        
    while (1) {
        read = fgets(required, len, stdin);
        if (read != NULL && (read_len = strlen(read)) > 1) {
            read[read_len - 1] = 0;   // 去掉换行
        } else if (opt != NULL) {
            strncpy(required, opt, len);
        } else {
            continue;
        }
        break;
    }
    return required;
}

bool process_cmd(const char *cmd, int argc, char **argv, void* userdata)
{
    xmpp_conn_t *conn = (xmpp_conn_t*)userdata;
    
    if (strncmp("quit", cmd, 4) == 0) {
        // # quit
        return false;
        
    } else if (strncmp("message", cmd, 7) == 0 && argc == 2) {
        // # message $who $message
        cmd_send_msg(conn, argv[0], argv[1]);
        
    } else if (strncmp("raw", cmd, 3) == 0 && argc == 1) {
        // # image $who $fullfilepath $imagetype
        xmpp_send_raw(conn, argv[0], strlen(argv[0]));
        
    } else if (strncmp("voice", cmd, 5) == 0 && argc == 4) {
        // # voice $who $fullfilepath $container $codec
        
    } else if (strncmp("call", cmd, 4) == 0 && argc == 1) {
        // # call $who $codec
        
    } else {
        printf("未知命令或者参数错误 \n");
    }
    
    return true;
}

void process_input(void *userdata)
{
#define buff_len 32+32*512

    char *read = NULL;
    size_t read_len = 0;
    char buff[buff_len];
    const char *promote = "# ";
    
    while (1) {
        printf(promote);
        read = fgets(buff, buff_len, stdin);
        if (read != NULL && (read_len = strlen(read)) > 1) {
            // 拆分命令
            int argc = 0;
            char cmd[32];                     // 命令类型最长32个字节
            char *param[32];                  // 最多支持32个参数
            const size_t max_param_len = 512; // 单个参数长度最多512
            
            char *cur = NULL;
            char *cur_param = NULL;
            int cur2 = 0;
            int state = 0;
            
            cur = read;
            while (1) {
                switch (state) {
                case 0: {
                    // 寻找命令起始
                    if (*cur != ' ') {
                        cur--;
                        memset(cmd, 0, sizeof(cmd));
                        cur2 = 0;
                        state = 1;
                    }
                }
                break;
                case 1: {
                    // 读取命令类型
                    if (*cur == ' ' || *cur == '\n') {
                        cur--;
                        state = 2;
                    } else if (cur2 == sizeof(cmd) - 1) {
                        break;  // break while(1) 解析命令
                    } else {
                        cmd[cur2++] = *cur;
                    }
                }
                break;
                case 2: {
                    // 寻找参数
                    if (*cur == ' ') {
                        // pass
                    } else if (*cur == '\n') {
                        break; // break while(1) 解析命令
                    } else {
                        // 开始读参数
                        if (*cur == '"') {
                            state = 3;
                        } else {
                            cur--;
                            state = 4;
                        }
                        cur2 = 0;
                        cur_param = (char*)malloc(max_param_len);
                        printf("%x\n", cur_param);
                        if (!cur_param) {
                            break; // break while(1) 解析命令
                        }
                        param[argc++] = cur_param;
                        memset(cur_param, 0, max_param_len*sizeof(char));
                    }
                }
                break;
                case 3: {
                    if (*cur == '"') {
                        state = 5;
                    } else {
                        cur_param[cur2++] = *cur;
                    }
                }
                break;
                case 4: {
                    if (*cur == ' ') {
                        state = 2;
                    } else {
                        cur_param[cur2++] = *cur;
                    }
                }
                break;
                case 5: {
                    if (*cur == ' ' || *cur == '\n') {
                        state = 2;
                    } else {
                        break; // break while(1) 解析命令
                    }
                }
                break;
                default:
                    break;
                }
                cur++;
                if (cur - read >= (int)read_len) {
                    break;
                }
            } // while(1) 解析命令
            
            // 执行命令
            if (strlen(cmd) > 0) {
                bool end = false;
                
                if (argc > 0) {
                    end = process_cmd(cmd, argc, param, userdata);
                    
                    // 释放
                    for (int i = 0; i < argc; i++) {
                        free(param[i]);
                    }
                } else {
                    end = process_cmd(cmd, 0, NULL, userdata);
                }
                
                if (!end) {
                    break;    // process_input
                }
            }
        }
    } // process_input
}

void *console_routine(void *arg)
{
    xmpp_conn_t *conn = (xmpp_conn_t*)arg;
    
    process_input(conn);
    xmpp_disconnect(conn);
    return NULL;
}

void conn_handler(xmpp_conn_t *conn, xmpp_conn_event_t status,
                  const int error, xmpp_stream_error_t *stream_error,
                  void *userdata)
{
    xmpp_ctx_t *ctx = (xmpp_ctx_t *)userdata;
    
    if (status == XMPP_CONN_CONNECT) {
        fprintf(stderr, "DEBUG: connected\n");
        // 上线
        xmpp_stanza_t *pres = xmpp_stanza_new(ctx);
        xmpp_stanza_set_name(pres, "presence");
        xmpp_send(conn, pres);
        xmpp_stanza_release(pres);
        
        // 消息处理器
        xmpp_handler_add(conn, handle_txt_msg, NULL, "message", NULL, ctx);
        
        // 启动控制台解析器
        int err = pthread_create(&console_thread, NULL, console_routine, conn);
        if (err != 0) {
            fprintf(stderr, "can't create console thread");
        }
    } else {
        fprintf(stderr, "DEBUG: disconnected\n");
        xmpp_stop(ctx);
    }
}

void *xmpp_routine(void *arg)
{
    xmpp_ctx_t *ctx = (xmpp_ctx_t *)arg;
    xmpp_run(ctx);
    return NULL;
}

void memcb(void *p, size_t s, const char *file, uint64_t line, void *mem_userdata,
           void *cb_userdata)
{
    printf("Memory leak at size %d, addr %x. %s, %d.  \n", s, p, file, line);
    free(p);
}

int main(int argc, char **argv)
{
    safe_mem_init();
    
    
    xmpp_ctx_t *ctx;
    xmpp_conn_t *conn;
    xmpp_log_t *log;
    
    char *jid = NULL, *pass = NULL, *host = NULL;
    
    if (argc > 1) {
        jid = argv[1];
    }
    if (argc > 2) {
        pass = argv[2];
    }
    if (argc > 3) {
        host = argv[3];
    }
    
    jid = require_string("输入账号", 64, jid);
    pass = require_string("输入密码", 64, pass);
    host = require_string("输入主机", 128, host);
    
    xmpp_initialize();
    
    if (test_thread(argc, argv)) {
        printf("test thread ok.\n");
    } else {
        printf("test thread fail.\n");
    }
    
    
    log = xmpp_get_default_logger(XMPP_LEVEL_DEBUG);
    ctx = xmpp_ctx_new(log);
    
    conn = xmpp_conn_new(ctx);
    
    xmpp_conn_set_jid(conn, jid);
    xmpp_conn_set_pass(conn, pass);
    
    //// 连接服务器
    xmpp_connect_client(conn, host, 0, conn_handler, ctx);
    
    int err = pthread_create(&xmpp_thread, NULL, xmpp_routine, ctx);
    if (err != 0) {
        printf("can't create xmpp thread: %s\n", strerror(err));
        return 1;
    }
    
    pthread_join(xmpp_thread, NULL);
    pthread_join(console_thread, NULL);
    
    xmpp_conn_release(conn);
    xmpp_ctx_free(ctx);
    xmpp_shutdown();
    
    safe_mem_free(jid);
    safe_mem_free(pass);
    if (host) {
        safe_mem_free(host);
    }
    
    safe_mem_check(memcb, NULL);
    _CrtDumpMemoryLeaks();
    
    printf("按任意按键关闭..");
    getchar();
    return 0;
};