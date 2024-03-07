#ifndef LIB_KERNEL_IO
#define LIB_KERNEL_IO

#include "stdint.h"

// 端口写一个字节
void outb(uint16_t port, uint8_t value);

// 端口读一个字节
uint8_t inb(uint16_t port);

// 端口读两个字节
uint16_t inw(uint16_t port);

/* 将addr处起始的word_cnt个字写入端口port */
void outsw(uint16_t port, const void* addr, uint32_t word_cnt);

/* 将从端口port读入的word_cnt个字写入addr */
void insw(uint16_t port, void* addr, uint32_t word_cnt);
#endif /* LIB_KERNEL_IO */
