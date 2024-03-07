#ifndef LIB_STDINT
#define LIB_STDINT
typedef signed char int8_t;
typedef signed short int int16_t;
typedef signed int int32_t;
typedef signed long long int int64_t;
typedef unsigned char uint8_t;
typedef unsigned short int uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long int uint64_t;
typedef unsigned int size_t;
typedef long int intptr_t;

// 用于类型转换 void* -> int
uint32_t voidptrTouint32(void* value);

void* uint32ToVoidptr(uint32_t value);
#endif /* LIB_STDINT */
