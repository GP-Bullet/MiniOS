#ifndef LIB_STRING
#define LIB_STRING

#include "stdint.h"

void memcpy(void *dest, const void *src, uint32_t len);

void memset(void *dest, uint8_t val, uint32_t len);

int memcmp(const void *a_, const void *b_, uint32_t size);

void bzero(void *dest, uint32_t len);

int strcmp(const char *str1, const char *str2);

char *strcpy(char *dest, const char *src);

char *strcat(char *dest, const char *src);

size_t strlen(const char *s);
char *strchr(const char *str, const uint8_t ch);
char *strrchr(const char *str, const uint8_t ch);
#endif /* LIB_STRING */
