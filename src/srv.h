/* sock.h
* socket��ֲ�����Լ���ʼ�����֣���д���ֽ���libevent
*/
#ifndef __IMCORE_SRV_H__
#define __IMCORE_SRV_H__

#include "common.h"
#include "sock.h"

// SRV��¼��ѯ
int im_srv_lookup(const char *service, const char *proto, const char *domain,
                  char *resulttarget, int resulttargetlength, int *resultport);

#endif // __IMCORE_SRV_H__ 
