#ifndef KERNEL_MEMORY
#define KERNEL_MEMORY
#include "bitmap.h"
#include "list.h"
#include "stdint.h"
#define PG_SIZE 4096  // 一个物理页的大小

/*虚拟地址池，用于虚拟地址管理*/
struct virtual_addr {
  struct bitmap vaddr_bitmap;  // 虚拟地址用到的位图结构
  uint32_t vaddr_start;        // 虚拟地址起始地址
};

extern struct pool kernel_pool, user_pool;

/* 内存池标记,用于判断用哪个内存池 */
enum pool_flags {
  PF_KERNEL = 1,  // 内核内存池
  PF_USER = 2     // 用户内存池
};

#define PG_P_1 1  // 页表项或页目录项存在属性位(示此页内存已存在。)
#define PG_P_0 0   // 页表项或页目录项存在属性位
#define PG_RW_R 0  // R/W 属性位值,读/执行
#define PG_RW_W 2  // R/W 属性位值,读/写/执行
#define PG_US_S 0  // U/S 属性位值,系统级
#define PG_US_U 4  // U/S 属性位值,用户级

/*内存块*/
struct mem_block {
  struct list_elem free_elem;
};

/*内存块描述符*/
struct mem_block_desc {
  uint32_t block_size;       // 内存块大小
  uint32_t block_per_arena;  // 该area可容纳mem_block的数量
  struct list free_list;     // 目前可用的mem_block链表
};

#define DESC_CNT 7  // 内存块描述符个数

void mem_init(void);

void* get_kernel_pages(uint32_t pg_cnt);
void* get_user_pages(uint32_t pg_cnt);
void* get_a_page(enum pool_flags pf, uint32_t vaddr);
uint32_t addr_v2p(uint32_t vaddr);
void block_desc_init(struct mem_block_desc* desc_array);
void pfree(uint32_t pg_phy_addr);
void* sys_malloc(uint32_t size);
void sys_free(void* ptr);
void* get_a_page_without_opvaddrbitmap(enum pool_flags pf, uint32_t vaddr);
void mfree_page(enum pool_flags pf, void* _vaddr, uint32_t pg_cnt);
void free_a_phy_page(uint32_t pg_phy_addr);
uint32_t* pte_ptr(uint32_t vaddr);
uint32_t* pde_ptr(uint32_t vaddr);
#endif /* KERNEL_MEMORY */