#ifndef LIB_STDIO
#define LIB_STDIO
#include "stdint.h"
typedef char* va_list;

#define va_start(ap, v) ap = (va_list)&v  // 把ap指向第一个固定参数v
#define va_arg(ap, t) *((t*)(ap += 4))  // ap 指向下一个参数并返回其值
#define va_end(ap) ap = NULL            // 清除 ap

uint32_t vsprintf(char* str, const char* format, va_list ap);
uint32_t printf(const char* format, ...);
uint32_t sprintf(char* buf, const char* format, ...);
#endif /* LIB_STDIO */
