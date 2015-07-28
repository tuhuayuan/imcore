/* sock.h
* socket移植定义以及初始化部分，读写部分交给libevent
*/
#ifndef __IMCORE_SRV_H__
#define __IMCORE_SRV_H__

#include "common.h"

// SRV记录查询
int im_srv_lookup(const char *service, const char *proto, const char *domain,
                  char *resulttarget, int resulttargetlength, int *resultport);

#endif // __IMCORE_SRV_H__ 
