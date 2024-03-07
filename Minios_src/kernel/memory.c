#include "memory.h"

#include "bitmap.h"
#include "debug.h"
#include "global.h"
#include "interrupt.h"
#include "list.h"
#include "print.h"
#include "string.h"
#include "sync.h"
#include "thread.h"

// 位图地址(一个4kb位图可以支持128MB内存)，9a000~9e000,4个页框大小的位图
#define MEM_BITMAP_BASE 0xc009a000

#define K_HEAP_START 0xc0100000

typedef struct pool {
  struct bitmap pool_bitmap;  // 本内存池用到的位图结构,用于管理物理内存
  uint32_t phy_addr_start;  // 本内存池所管理物理内存的起始地址
  uint32_t pool_size;       // 本内存池字节容量
  struct lock lock;         // 申请内存时互斥
} pool;

/*内存仓库*/
struct arena {
  struct mem_block_desc* desc;  // 此arena关联的mem_block_desc
  uint32_t cnt;
  bool large;  // 为true,cnt表示页框数,为false表示mem_block数量
};

// 内核内存块描述符数组
struct mem_block_desc k_block_descs[DESC_CNT];

pool kernel_pool, user_pool;       // 生成内核内存池和用户内存池
struct virtual_addr kernel_vaddr;  // 此结构用来给内核分配虚拟地址

// 返回高10位索引
#define PDE_IDX(addr) ((addr & 0xffc00000) >> 22)

// 返回中间10位索引
#define PTE_IDX(addr) ((addr & 0x003ff000) >> 12)

/*初始化内存池*/
static void mem_pool_init(uint32_t all_mem) {
  put_str(" mem_pool_init start\n");
  //(一个目录表+第一个物理页+第 769~1022 个页目录项共指向 254 个页表=256)
  uint32_t page_table_size = PG_SIZE * 256;
  // 已经使用的内存，低端1M+已经映射的页表占用的
  uint32_t used_mem = page_table_size + 0x100000;
  uint32_t free_mem = all_mem - used_mem;

  uint16_t all_free_pages = free_mem / PG_SIZE;
  uint16_t kernel_free_pages = all_free_pages / 2;
  uint16_t user_free_pages = all_free_pages - kernel_free_pages;

  // 取整，不算余数(简化bitmap操作，但会造成内存浪费[余数不会算进内存]，好处是不用担心越界)
  uint32_t kbm_length = kernel_free_pages / 8;  // Kernel BitMap 的长度
  uint32_t ubm_length = user_free_pages / 8;    // User BitMap 的长度

  uint32_t kp_start = used_mem;  // 内核起始地址
  uint32_t up_start = kp_start + kernel_free_pages * PG_SIZE;  // 用户起始地址

  kernel_pool.phy_addr_start = kp_start;
  kernel_pool.pool_size = kernel_free_pages * PG_SIZE;
  kernel_pool.pool_bitmap.btmp_bytes_len = kbm_length;

  user_pool.phy_addr_start = up_start;
  user_pool.pool_size = user_free_pages * PG_SIZE;
  user_pool.pool_bitmap.btmp_bytes_len = ubm_length;

  kernel_pool.pool_bitmap.bits = (void*)MEM_BITMAP_BASE;
  user_pool.pool_bitmap.bits = uint32ToVoidptr(MEM_BITMAP_BASE + kbm_length);

  /*输出内存池信息*/
  put_str("  kernel_pool_bitmap_start:");
  put_int(voidptrTouint32((void*)(kernel_pool.pool_bitmap.bits)));
  put_str("  kernel_pool_phy_addr_start:");
  put_int(kernel_pool.phy_addr_start);
  put_str("\n");

  put_str("  user_pool_bitmap_start:");
  put_int(voidptrTouint32((void*)(user_pool.pool_bitmap.bits)));
  put_str("  user_pool_phy_addr_start:");
  put_int(user_pool.phy_addr_start);
  put_str("\n");

  // 初始化位图
  bitmap_init(&kernel_pool.pool_bitmap);
  bitmap_init(&kernel_pool.pool_bitmap);

  // 初始化内核虚拟地址的位图
  kernel_vaddr.vaddr_bitmap.btmp_bytes_len = kbm_length;
  kernel_vaddr.vaddr_bitmap.bits =
      uint32ToVoidptr(MEM_BITMAP_BASE + kbm_length + ubm_length);
  kernel_vaddr.vaddr_start = K_HEAP_START;
  bitmap_init(&kernel_vaddr.vaddr_bitmap);

  // 锁初始化
  lock_init(&kernel_pool.lock);
  lock_init(&user_pool.lock);

  put_str("  kernel_vaddr_bitmap_start:");
  put_int(voidptrTouint32((void*)(kernel_vaddr.vaddr_bitmap.bits)));
  put_str("\n");

  put_str(" mem_pool_init done\n");
}

// 在pf标志的虚拟内存池中申请pg_cnt个虚拟页(虚拟地址的空间要保证连续，所以提供该功能)
// 成功返回首地址，失败返回NULL
static void* vaddr_get(enum pool_flags pf, uint32_t pg_cnt) {
  int vaddr_start = 0, bit_idx_start = -1;
  uint32_t cnt = 0;

  if (pf == PF_KERNEL) {
    bit_idx_start = bitmap_scan(&kernel_vaddr.vaddr_bitmap, pg_cnt);
    if (bit_idx_start == -1) {  // 失败
      return NULL;
    }
    while (cnt < pg_cnt) {  // 将寻找到的空位置标记为1
      bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_idx_start + cnt++, 1);
    }
    vaddr_start = kernel_vaddr.vaddr_start + bit_idx_start * PG_SIZE;
  } else {  // 用户内存池
    struct task_struct* cur = running_thread();
    bit_idx_start = bitmap_scan(&cur->userprog_vaddr.vaddr_bitmap, pg_cnt);
    if (bit_idx_start == -1) {
      return NULL;
    }
    while (cnt < pg_cnt) {
      bitmap_set(&cur->userprog_vaddr.vaddr_bitmap, bit_idx_start + cnt++, 1);
    }
    vaddr_start = cur->userprog_vaddr.vaddr_start + bit_idx_start * PG_SIZE;
    // 这块内核空间地址已经被分
    ASSERT((uint32_t)vaddr_start < (0xc0000000 - PG_SIZE));
  }
  return uint32ToVoidptr(vaddr_start);
}

/* 在虚拟地址池中释放以_vaddr 起始的连续 pg_cnt 个虚拟页地址 */
static void vaddr_remove(enum pool_flags pf, void* _vaddr, uint32_t pg_cnt) {
  uint32_t bit_idx_start = 0;
  uint32_t vaddr = (uint32_t)_vaddr;
  uint32_t cnt = 0;
  if (pf == PF_KERNEL) {
    bit_idx_start = (vaddr - kernel_vaddr.vaddr_start) / PG_SIZE;
    while (cnt < pg_cnt) {
      bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_idx_start + cnt++, 0);
    }
  } else {
    struct task_struct* cur_thread = running_thread();
    bit_idx_start = (vaddr - cur_thread->userprog_vaddr.vaddr_start) / PG_SIZE;
    while (cnt < pg_cnt) {
      bitmap_set(&cur_thread->userprog_vaddr.vaddr_bitmap,
                 bit_idx_start + cnt++, 0);
    }
  }
}

/* 得到虚拟地址 vaddr 对应的 pte 指针*/
uint32_t* pte_ptr(uint32_t vaddr) {
  uint32_t* pte = uint32ToVoidptr(0xffc00000 + ((vaddr & 0xffc00000) >> 10) +
                                  PTE_IDX(vaddr) * 4);
  return pte;
}

/* 得到虚拟地址 vaddr 对应的 pde 指针*/
uint32_t* pde_ptr(uint32_t vaddr) {
  uint32_t* pde = uint32ToVoidptr((0xfffff000) + PDE_IDX(vaddr) * 4);
  return pde;
}

/* 在 m_pool 指向的物理内存池中分配 1 个物理页, *
 * 成功则返回页框的物理地址,失败则返回 NULL */
static void* palloc(struct pool* m_pool) {
  /* 扫描或设置位图要保证原子操作 */
  int bit_idx = bitmap_scan(&m_pool->pool_bitmap, 1);  // 找一个物理页面
  if (bit_idx == -1) {
    return NULL;
  }
  bitmap_set(&m_pool->pool_bitmap, bit_idx, 1);  // 将此位 bit_idx 置 1

  uint32_t page_phyaddr = ((bit_idx * PG_SIZE) + m_pool->phy_addr_start);

  return uint32ToVoidptr(page_phyaddr);
}

/*将物理地址pg_phy_addr 回收到物理内存池*/
void pfree(uint32_t pg_phy_addr) {
  struct pool* mem_pool;
  uint32_t bit_idx = 0;
  if (pg_phy_addr >= user_pool.phy_addr_start) {
    mem_pool = &user_pool;
    bit_idx = (pg_phy_addr - user_pool.phy_addr_start) / PG_SIZE;
  } else {
    mem_pool = &kernel_pool;
    bit_idx = (pg_phy_addr - kernel_pool.phy_addr_start) / PG_SIZE;
  }
  bitmap_set(&mem_pool->pool_bitmap, bit_idx, 0);  // 将位图中该位置清零
}

/* 页表中添加虚拟地址_vaddr 与物理地址_page_phyaddr 的映射 */
static void page_table_add(void* _vaddr, void* _page_phyaddr) {
  uint32_t vaddr = voidptrTouint32(_vaddr);
  uint32_t page_phyaddr = voidptrTouint32(_page_phyaddr);

  uint32_t* pde = pde_ptr(vaddr);
  uint32_t* pte = pte_ptr(vaddr);

  if (*pde & 0x00000001) {  // 已经存在
    ASSERT(!(*pte & 0x00000001));
    if (!(*pte & 0x00000001)) {
      // 只要是创建页表,pte 就应该不存在,多判断一下放心
      *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
    } else {  // 目前应该不会执行到这,因为上面的 ASSERT 会先执行
      PANIC("pte repeat");
      // *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);  //
    }
  } else {  // 也目录项不存在，所以要先创建页目录再创建页表项
            // 页表用到的页框从内核分配
    uint32_t pde_phyaddr = voidptrTouint32(palloc(&kernel_pool));
    *pde = (pde_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
    // 将还页框里面的脏数据清零
    // pte高20位指向该pte所在页框的首地址
    void* page_start = uint32ToVoidptr(voidptrTouint32(pte) & 0xfffff000);
    memset(page_start, 0, PG_SIZE);
    ASSERT(!(*pte & 0x00000001));
    *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
  }
}

static void page_table_pte_remove(uint32_t vaddr) {
  uint32_t* pte = pte_ptr(vaddr);
  *pte &= ~PG_P_1;                                    // 将p位置0
  asm volatile("invlpg %0" ::"m"(vaddr) : "memory");  // 更新 tlb(页表高速缓存)
}

/* 分配 pg_cnt 个页空间,成功则返回起始虚拟地址,失败时返回 NULL */
void* malloc_page(enum pool_flags pf, uint32_t pg_cnt) {
  ASSERT(pg_cnt > 0 &&
         pg_cnt < 3840);  // 内存大小16MB(用15MB进行约束，3840个页)
  void* vaddr_start = vaddr_get(pf, pg_cnt);
  if (vaddr_start == NULL) {
    return NULL;
  }
  uint32_t vaddr = voidptrTouint32(vaddr_start);
  uint32_t cnt = pg_cnt;

  pool* mem_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;
  /* 因为虚拟地址是连续的,但物理地址可以是不连续的,所以逐个做映射*/
  while (cnt-- > 0) {
    void* page_phyaddr = palloc(mem_pool);
    if (page_phyaddr == NULL) {
      // 此处失败，失败时要将曾经已经完成的申请的虚拟地址和物理进行回收，将来实现free时完成
      return NULL;
    }
    page_table_add(uint32ToVoidptr(vaddr), page_phyaddr);
    vaddr += PG_SIZE;  // 下一个虚拟页
  }
  return vaddr_start;
}

/*释放以虚拟地址 vaddr 为起始的 cnt 个物理页框 */
void mfree_page(enum pool_flags pf, void* _vaddr, uint32_t pg_cnt) {
  uint32_t pg_phy_addr;
  uint32_t vaddr = (int32_t)_vaddr;
  uint32_t page_cnt = 0;
  ASSERT(pg_cnt >= 1 && vaddr % PG_SIZE == 0);  // 页框要对齐
  pg_phy_addr = addr_v2p(vaddr);                // 获取物理地址

  // 确保页框对齐，并且地址不是低端1M和内核页目录和第一个页表
  ASSERT((pg_phy_addr % PG_SIZE) == 0 && pg_phy_addr >= 0x102000);
  if (pg_phy_addr >= user_pool.phy_addr_start) {
    vaddr -= PG_SIZE;
    while (page_cnt < pg_cnt) {
      vaddr += PG_SIZE;
      pg_phy_addr = addr_v2p(vaddr);
      /* 确保物理地址属于用户物理内存池 */
      ASSERT((pg_phy_addr % PG_SIZE) == 0 &&
             pg_phy_addr >= user_pool.phy_addr_start);
      // 将物理页框归还内存池
      pfree(pg_phy_addr);
      /* 再从页表中清除此虚拟地址所在的页表项 pte */
      page_table_pte_remove(vaddr);
      page_cnt++;
    }
    vaddr_remove(pf, _vaddr, pg_cnt);
  } else {  // 内核内存池
    vaddr -= PG_SIZE;
    while (page_cnt < pg_cnt) {
      vaddr += PG_SIZE;
      pg_phy_addr = addr_v2p(vaddr);
      /* 确保物理地址属于内核物理内存池 */
      ASSERT((pg_phy_addr % PG_SIZE) == 0 &&
             pg_phy_addr >= kernel_pool.phy_addr_start &&
             pg_phy_addr < user_pool.phy_addr_start);
      // 将物理页框归还内存池
      pfree(pg_phy_addr);
      /* 再从页表中清除此虚拟地址所在的页表项 pte */
      page_table_pte_remove(vaddr);
      page_cnt++;
    }
    vaddr_remove(pf, _vaddr, pg_cnt);
  }
}

/* 从内核物理内存池中申请 cnt 页内存,
成功则返回其虚拟地址,失败则返回 NULL */
void* get_kernel_pages(uint32_t pg_cnt) {
  void* vaddr = malloc_page(PF_KERNEL, pg_cnt);
  if (vaddr != NULL) {  // 若分配的地址不为空,将页框清 0(清除脏数据) 后返回
    memset(vaddr, 0, pg_cnt * PG_SIZE);
  }
  return vaddr;
}

void* get_a_page(enum pool_flags pf, uint32_t vaddr) {
  pool* mem_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;
  lock_acquire(&mem_pool->lock);

  struct task_struct* cur = running_thread();
  int32_t bit_idx = -1;

  /* 若当前是用户进程申请用户内存,就修改用户进程自己的虚拟地址位图 */
  if (cur->pgdir != NULL && pf == PF_USER) {
    bit_idx = (vaddr - cur->userprog_vaddr.vaddr_start) / PG_SIZE;
    ASSERT(bit_idx > 0);
    bitmap_set(&cur->userprog_vaddr.vaddr_bitmap, bit_idx, 1);
  } else if (cur->pgdir == NULL && pf == PF_KERNEL) {
    bit_idx = (vaddr - kernel_vaddr.vaddr_start) / PG_SIZE;
    ASSERT(bit_idx > 0);
    bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_idx, 1);
  } else {
    PANIC(
        "get_a_page:not allow kernel alloc userspace or user alloc "
        "kernelspace "
        "by get_a_page");
  }

  void* page_phyaddr = palloc(mem_pool);
  if (page_phyaddr == NULL) {
    return NULL;
  }
  page_table_add((void*)vaddr, page_phyaddr);
  lock_release(&mem_pool->lock);
  return (void*)vaddr;
}

/*得到虚拟地址映射的物理地址*/
uint32_t addr_v2p(uint32_t vaddr) {
  uint32_t* pte = pte_ptr(vaddr);

  /*pte指针存放的就是物理页项条目*/
  /*去掉低12位置的页表属性+再加上虚拟地址的低12(页内偏移)*/
  return ((*pte & 0xfffff000) + (vaddr & 0x00000fff));
}

/*从用户空间中申请4k内存，并返回其虚拟地址*/
void* get_user_pages(uint32_t pg_cnt) {
  lock_acquire(&user_pool.lock);
  void* vaddr = malloc_page(PF_USER, pg_cnt);
  if (vaddr != NULL) {
    memset(vaddr, 0, pg_cnt * PG_SIZE);
  }
  lock_release(&user_pool.lock);
  return vaddr;
}

/*初始化为malloc做准备*/
void block_desc_init(struct mem_block_desc* desc_array) {
  uint16_t desc_idx, block_size = 16;
  /*初始化每个mem_block_desc描述符*/
  for (desc_idx = 0; desc_idx < DESC_CNT; desc_idx++) {
    desc_array[desc_idx].block_size = block_size;
    desc_array[desc_idx].block_per_arena =
        (PG_SIZE - sizeof(struct arena)) / block_size;

    list_init(&desc_array[desc_idx].free_list);

    block_size *= 2;
  }
}

/*返回arena中的第idx个内存块地址*/
static struct mem_block* arena2block(struct arena* a, uint32_t idx) {
  return (struct mem_block*)((uint32_t)a + sizeof(struct arena) +
                             idx * a->desc->block_size);
}

/*返回内存块b所在的arena地址(1024向上取整)*/
static struct arena* block2arena(struct mem_block* b) {
  return (struct arena*)((uint32_t)b & 0xfffff000);
}

/*在堆区申请size字节内存*/
void* sys_malloc(uint32_t size) {
  enum pool_flags PF;
  struct pool* mem_pool;
  uint32_t pool_size;
  struct mem_block_desc* descs;
  struct task_struct* cur_thread = running_thread();
  /*判断使用哪一个内存池*/
  if (cur_thread->pgdir == NULL) {  // 内核线程
    PF = PF_KERNEL;
    mem_pool = &kernel_pool;
    pool_size = kernel_pool.pool_size;
    descs = k_block_descs;
  } else {
    PF = PF_USER;
    mem_pool = &user_pool;
    pool_size = user_pool.pool_size;
    descs = cur_thread->u_block_desc;
  }

  /* 若申请的内存不在内存池容量范围内,则直接返回 NULL */
  if (!(size > 0 && size < pool_size)) {
    return NULL;
  }

  struct arena* a;
  struct mem_block* b;
  lock_acquire(&mem_pool->lock);

  /*超过最大内存块1024,就分配页框*/
  if (size > 1024) {
    uint32_t page_cnt = DIV_ROUND_UP(size + sizeof(struct arena), PG_SIZE);
    a = malloc_page(PF, page_cnt);
    if (a != NULL) {
      memset(a, 0, page_cnt * PG_SIZE);  // 清除脏数据
      a->desc = NULL;                    // 大于1024,为large
      a->cnt = page_cnt;
      a->large = true;
      lock_release(&mem_pool->lock);
      return (void*)(a + 1);  // 跨过 arena 大小,把剩下的内存返回
    } else {                  // 申请失败
      lock_release(&mem_pool->lock);
      return NULL;
    }
  } else {
    uint8_t desc_idx;
    // 选区规格的内存块
    for (desc_idx = 0; desc_idx < DESC_CNT; desc_idx++) {
      if (size <= descs[desc_idx].block_size) {
        // 从小往大找,找到后退出
        break;
      }
    }

    // 如果空闲队列为空
    if (list_empty(&descs[desc_idx].free_list)) {
      a = malloc_page(PF, 1);
      if (a == NULL) {
        lock_release(&mem_pool->lock);
        return NULL;
      }
      memset(a, 0, PG_SIZE);

      /* 对于分配的小块内存,将 desc 置为相应内存块描述符,
       * cnt 置为此 arena 可用的内存块数,large 置为 false */
      a->desc = &descs[desc_idx];
      a->large = false;
      a->cnt = descs[desc_idx].block_per_arena;
      uint32_t block_idx;
      enum intr_status old_status = intr_disable();

      for (block_idx = 0; block_idx < descs[desc_idx].block_per_arena;
           block_idx++) {
        b = arena2block(a, block_idx);
        ASSERT(!elem_find(&a->desc->free_list, &b->free_elem));
        list_append(&a->desc->free_list, &b->free_elem);
      }
      intr_set_status(old_status);
    }

    /* 开始分配内存块 */
    b = elem2entry(struct mem_block, free_elem,
                   list_pop(&(descs[desc_idx].free_list)));
    memset(b, 0, descs[desc_idx].block_size);

    a = block2arena(b);  // 获取内存块 b 所在的 arena
    a->cnt--;            // 将此 arena 中的空闲内存块数减 1
    lock_release(&mem_pool->lock);
    return (void*)b;
  }
}

/*回收内存ptr*/
void sys_free(void* ptr) {
  ASSERT(ptr != NULL);
  if (ptr != NULL) {
    enum pool_flags PF;
    struct pool* mem_pool;

    /*判断是线程还是进程*/
    if (running_thread()->pgdir == NULL) {
      ASSERT((uint32_t)ptr >= K_HEAP_START);
      PF = PF_KERNEL;
      mem_pool = &kernel_pool;
    } else {
      PF = PF_USER;
      mem_pool = &user_pool;
    }
    lock_acquire(&mem_pool->lock);
    struct mem_block* b = ptr;
    struct arena* a = block2arena(b);
    ASSERT(a->large == false || a->large == true);
    if (a->desc == NULL && a->large == true) {  // 大于1024
      mfree_page(PF, a, a->cnt);                // 释放
    } else {
      /*先回收到free_list中*/
      list_append(&a->desc->free_list, &b->free_elem);

      /*再判断此arena中的内存是否都是空闲,如果是那就全部释放*/
      if (++(a->cnt) == a->desc->block_per_arena) {
        uint32_t block_idx;
        for (block_idx = 0; block_idx < a->desc->block_per_arena; block_idx++) {
          struct mem_block* b = arena2block(a, block_idx);
          ASSERT(elem_find(&a->desc->free_list, &b->free_elem));
          list_remove(&b->free_elem);
        }
        mfree_page(PF, a, 1);
      }
    }
    lock_release(&mem_pool->lock);
  }
}

/* 根据物理页框地址 pg_phy_addr 在相应的内存池的位图清 0,不改动页表*/
void free_a_phy_page(uint32_t pg_phy_addr) {
  struct pool* mem_pool;
  uint32_t bit_idx = 0;
  if (pg_phy_addr >= user_pool.phy_addr_start) {
    mem_pool = &user_pool;
    bit_idx = (pg_phy_addr - user_pool.phy_addr_start) / PG_SIZE;
  } else {
    mem_pool = &kernel_pool;
    bit_idx = (pg_phy_addr - kernel_pool.phy_addr_start) / PG_SIZE;
  }
  bitmap_set(&mem_pool->pool_bitmap, bit_idx, 0);
}

/* 安装 1 页大小的 vaddr,专门针对 fork 时虚拟地址位图无需操作的情况 */
void* get_a_page_without_opvaddrbitmap(enum pool_flags pf, uint32_t vaddr) {
  struct pool* mem_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;
  lock_acquire(&mem_pool->lock);
  void* page_phyvaddr = palloc(mem_pool);
  if (page_phyvaddr == NULL) {
    lock_release(&mem_pool->lock);
    return NULL;
  }
  page_table_add((void*)vaddr, page_phyvaddr);
  lock_release(&mem_pool->lock);
  return (void*)vaddr;
}

void mem_init(void) {
  put_str("mem_init start\n");
  uint32_t mem_bytes_total = (*(uint32_t*)(0xb00));
  mem_pool_init(mem_bytes_total);  // 初始化内存池
  block_desc_init(k_block_descs);
  put_str("mem_init done\n");
}