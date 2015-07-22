#ifndef __IMCORE_INL_H__
#define __IMCORE_INL_H__

#include "imcore.h"
#include "common.h"

// 基本信息定义
struct im_msg {
    uint64_t id;
    char *form;
    char *to;
    time_t createtime;
};

#endif // __IMCORE_INL_H__