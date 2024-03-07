#ifndef __BITMAP_H_
#define __BITMAP_H_
#include "global.h"

#define BITMAP_MASK 1

// 遍历位图，整体以字节为单位，细节上以位为单位，所以此处位图的指针必须是单字节
struct bitmap {
  uint32_t btmp_bytes_len;
  uint8_t* bits;
};

void bitmap_init(struct bitmap* btmap);
bool bitmap_scan_test(struct bitmap* btmap, uint32_t bit_idx);
int bitmap_scan(struct bitmap* btmp, uint32_t cnt);
void bitmap_set(struct bitmap* btmp, uint32_t bit_idx, int8_t value);
#endif /*__BITMAP_H_*/