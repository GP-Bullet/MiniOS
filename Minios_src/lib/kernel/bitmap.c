#include "bitmap.h"

#include "debug.h"
#include "interrupt.h"
#include "print.h"
#include "string.h"

/*将位图btmap初始化*/
void bitmap_init(struct bitmap *btmap) {
  memset(btmap->bits, 0, btmap->btmp_bytes_len);
}

/* 判断 bit_idx 位是否为 1,若为 1,则返回 true,否则返回 false */
bool bitmap_scan_test(struct bitmap *btmap, uint32_t bit_idx) {
  uint32_t byte_idx = bit_idx / 8;  // 向下取整用于索引数组下标
  uint32_t bit_odd = bit_idx % 8;   // 取余用于索引数组内的位
  return (btmap->bits[byte_idx] & (BITMAP_MASK << bit_odd));
}

/*在位图中申请连续 cnt 个位,成功,则返回其起始位下标,失败,返回−1*/
int bitmap_scan(struct bitmap *btmp, uint32_t cnt) {
  uint32_t idx_byte = 0;  // 数组下标索引
  // 寻找一个未分配完的数组下标(逐字节查找)
  while ((0xff == btmp->bits[idx_byte]) && (idx_byte < btmp->btmp_bytes_len)) {
    idx_byte++;
  }
  ASSERT(idx_byte < btmp->btmp_bytes_len);
  if (idx_byte == btmp->btmp_bytes_len) {
    return -1;  // 若该内存池找不到可用空间
  }

  int idx_bit = 0;  // 以bit为单位的索引
  // 寻找一个未分配的bit(逐bit查找)
  while ((uint8_t)(BITMAP_MASK << idx_bit) & btmp->bits[idx_byte]) {
    idx_bit++;
  }
  int bit_idx_start = idx_byte * 8 + idx_bit;
  if (cnt == 1) {
    return bit_idx_start;
  }
  uint32_t bit_left = (btmp->btmp_bytes_len * 8 - bit_idx_start);  // 剩余的位
  uint32_t next_bit = bit_idx_start + 1;
  uint32_t count = 1;  // 用于记录找到的空闲位置个数
  bit_idx_start = -1;  // 先将其置为−1,若找不到连续的位就直接返回

  while (bit_left-- > 0) {
    if (!(bitmap_scan_test(btmp, next_bit))) {  // 若next_bit为0
      count++;
    } else {  // 若next_bit为1
      count = 0;
    }
    if (count == cnt) {  // 找到连续的cnt个空位置
      bit_idx_start = next_bit - cnt + 1;
      break;
    }
    next_bit++;
  }
  return bit_idx_start;
}

/* 将位图 btmp 的 bit_idx 位设置为 value */
void bitmap_set(struct bitmap *btmp, uint32_t bit_idx, int8_t value) {
  ASSERT((value == 0) || (value == 1));
  uint32_t byte_idx = bit_idx / 8;  // 向下取整用于索引数组下标
  uint32_t bit_odd = bit_idx % 8;   // 取余用于索引数组内的位
  if (value) {
    btmp->bits[byte_idx] |= (uint8_t)(BITMAP_MASK << bit_odd);
  } else {
    btmp->bits[byte_idx] &= ~(uint8_t)(BITMAP_MASK << bit_odd);
  }
}