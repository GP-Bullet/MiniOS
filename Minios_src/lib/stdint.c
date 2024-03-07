#include "stdint.h"
uint32_t voidptrTouint32(void* value) { return (uint32_t)(long int)value; }

void* uint32ToVoidptr(uint32_t value) { return (void*)((long int)value); }