/* base64.h
 * base64编解码实现 RFC 3548
*/
#ifndef __IMCORE_BASE64_H__
#define __IMCORE_BASE64_H__

#include <stdlib.h>
#include <stdint.h>

// 编码
int base64_encoded_len(size_t len);
char *base64_encode(const char *buffer, size_t len);

// 解码
int base64_decoded_len(const char *buffer, size_t len);
unsigned char *base64_decode(const char *buffer,size_t len);

#endif // __IMCORE_BASE64_H__