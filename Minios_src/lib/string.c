#include "string.h"

#include "debug.h"
#include "global.h"
#include <assert.h>
void memcpy(void *dest, const void *src, uint32_t len) {
  assert(dest != NULL && src != NULL);
  uint8_t *To = (uint8_t *)dest;
  uint8_t *From = (uint8_t *)src;
  while (len-- > 0) {
    *To++ = *From++;
  }
}

void memset(void *dest, uint8_t val, uint32_t len) {
  assert(dest != NULL);
  uint8_t *dst = (uint8_t *)dest;
  while (len-- > 0) {
    *dst++ = val;
  }
}

int memcmp(const void *a_, const void *b_, uint32_t size) {
  const char *a = a_;
  const char *b = b_;
  assert(a != NULL || b != NULL);
  while (size-- > 0) {
    if (*a != *b) {
      return *a - *b;
    }
    a++;
    b++;
  }
  return 0;
}
void bzero(void *dest, uint32_t len) { memset(dest, 0, len); }

int strcmp(const char *str1, const char *str2) {
  assert(str1 != NULL || str2 != NULL);
  while ((*str1 == *str2) && (*str1 != '\0')) {
    str1++;
    str2++;
  }
  return *str1 - *str2;
}

char *strcpy(char *dest, const char *src) {
  assert(dest != NULL && src != NULL);
  char *temp = dest;
  while (*src) {
    *dest++ = *src++;
  }
  return temp;
}

size_t strlen(const char *s) {
  assert(s != NULL);
  const char *p = s;
  while (*p++) {
  }
  return (p - s - 1);
}

/*首次出现ch的地址*/
char *strchr(const char *str, const uint8_t ch) {
  assert(str != NULL);
  while (*str != 0) {
    if (*str == ch) {
      return (char *)str;
    }
    str++;
  }
  return NULL;
}

/* 将字符串src_拼接到dst_后,将回拼接的串地址 */
char *strcat(char *dst_, const char *src_) {
  assert(dst_ != NULL && src_ != NULL);
  char *str = dst_;
  while (*str++)
    ;
  --str;  // 别看错了，--str是独立的一句，并不是while的循环体
  while ((*str++ = *src_++))
    ;  // 当*str被赋值为0时,此时表达式不成立,正好添加了字符串结尾的0.
  return dst_;
}

char *strrchr(const char *str, const uint8_t ch) {
  assert(str != NULL);
  char *temp = NULL;
  while (*str != 0) {
    if (*str == ch) {
      temp = (char *)str;
    }
    str++;
  }
  return temp;
}