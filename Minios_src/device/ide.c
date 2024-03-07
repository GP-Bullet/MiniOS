#include "ide.h"

#include <stdio.h>

#include "debug.h"
#include "interrupt.h"
#include "io.h"
#include "memory.h"
#include "stdio_kernel.h"
#include "string.h"
#include "timer.h"
/*Command Block
 * registers(用于向硬盘驱动器写入命令字或者从硬盘控制器获得硬盘状态)*/
#define reg_data(channel) (channel->port_base + 0)
#define reg_error(channel) (channel->port_base + 1)
#define reg_sect_cnt(channel) (channel->port_base + 2)  // 存放读写扇区
#define reg_lba_l(channel) (channel->port_base + 3)
#define reg_lba_m(channel) (channel->port_base + 4)
#define reg_lba_h(channel) (channel->port_base + 5)
#define reg_dev(channel) (channel->port_base + 6)
#define reg_status(channel) (channel->port_base + 7)
#define reg_cmd(channel) (reg_status(channel))
/*Control Block registers(控制硬盘工作状态)*/
#define reg_alt_status(channel) (channel->port_base + 0x206)
#define reg_ctl(channel) reg_alt_status(channel)

/* reg_alt_status 寄存器的一些关键位 */
#define BIT_ALT_STAT_BSY 0x80   // 硬盘忙
#define BIT_ALT_STAT_DRDY 0x40  // 驱动器准备好
#define BIT_ALT_STAT_DRQ 0x8    // 数据传输准备好了

/* device 寄存器的一些关键位 */
#define BIT_DEV_MBS 0xa0  // 第 7 位和第 5 位固定为 1
#define BIT_DEV_LBA 0x40  // 第 6 位寻址模式(LBA)
#define BIT_DEV_DEV 0x10  // 第 4 位主盘或从盘(主盘)

/*一些硬盘操作的指令*/
#define CMD_IDENTIFY 0xec      // identufy指令
#define CMD_READ_SECTOR 0x20   // 读扇区指令
#define CMD_WRITE_SECTOR 0x30  // 写扇区指令

/* 定义可读写的最大扇区数,调试用的 */
#define max_lba ((80 * 1024 * 1024 / 512) - 1)  // 只支持 80MB 硬盘

uint8_t channel_cnt;             // 按硬盘数计算的通道数
struct ide_channel channels[2];  // 有两个ide通道

/* 用于记录总扩展分区的起始lba,初始为 0,partition_scan 时以此为标记 */
int32_t ext_lba_base = 0;

uint8_t p_no = 0, l_no = 0;  // 用来记录硬盘主分区和逻辑分区的下标

struct list partition_list;  // 分区队列

/*构建一个16字节大小的结构体，用来存分区表项*/
struct partition_table_entry {
  uint8_t bootable;    // 是否可引导
  uint8_t start_head;  // 起始磁头号
  uint8_t start_sec;   // 起始扇区号
  uint8_t start_chs;   // 起始柱面号
  uint8_t fs_type;     // 分区类型
  uint8_t end_head;    // 结束磁头号
  uint8_t end_sec;     // 结束扇区号
  uint8_t end_chs;     // 结束柱面号
  /*重点关注的是下面两项*/
  uint32_t start_lba;       // 本分区起始扇区的lba地址
  uint32_t sec_cnt;         // 本分区的扇区数目
} __attribute__((packed));  // gcc拓展语法，保证这个结构体占16字节

/*引导扇区mbr或ebr*/
struct boot_sector {
  uint8_t other[446];  // 引导代码
  struct partition_table_entry partition_table[4];
  uint16_t signature;  // 启动扇区的结束标志0x55,0xaa
} __attribute__((packed));

/*选择的读写的硬盘*/
static void select_disk(struct disk* hd) {
  uint8_t reg_device = BIT_DEV_MBS | BIT_DEV_LBA;  // 固定位和LBA
  if (hd->dev_no == 1) {                           // 判断主从盘
    reg_device |= BIT_DEV_DEV;
  }
  outb(reg_dev(hd->my_channel), reg_device);
}

/* 向硬盘控制器写入起始扇区地址及要读写的扇区数 */
static void select_sector(struct disk* hd, uint32_t lba, uint8_t sec_cnt) {
  ASSERT(lba <= max_lba);
  struct ide_channel* channel = hd->my_channel;
  /*写入要读入的扇区数(0,表示256)*/
  outb(reg_sect_cnt(channel), sec_cnt);

  /*写入lba地址，及扇区号*/
  outb(reg_lba_l(channel), lba);
  outb(reg_lba_m(channel), lba >> 8);
  outb(reg_lba_h(channel), lba >> 16);

  outb(reg_dev(channel), BIT_DEV_MBS | BIT_DEV_LBA |
                             (hd->dev_no == 1 ? BIT_DEV_DEV : 0) | lba >> 24);
}

/* 向通道 channel 发命令 cmd */
static void cmd_out(struct ide_channel* channel, uint8_t cmd) {
  channel->expecting_intr = true;  // 置为true,告诉中断处理程序，正在等待cmd结果
  outb(reg_cmd(channel), cmd);
}

/*硬盘读入sec_cnt个数据到buf*/
static void read_from_sector(struct disk* hd, void* buf, uint8_t sec_cnt) {
  uint32_t size_in_byte;
  if (sec_cnt == 0) {
    size_in_byte = 256 * 512;
  } else {
    size_in_byte = sec_cnt * 512;
  }

  insw(reg_data(hd->my_channel), buf, size_in_byte / 2);
}

/* 将 buf 中 sec_cnt 扇区的数据写入硬盘 */
static void write_to_sector(struct disk* hd, void* buf, uint8_t sec_cnt) {
  uint32_t size_in_byte;
  if (sec_cnt == 0) {
    size_in_byte = 256 * 512;
  } else {
    size_in_byte = sec_cnt * 512;
  }

  outsw(reg_data(hd->my_channel), buf, size_in_byte / 2);
}

/*等待30s(每10ms检查一次)*/
static bool busy_wait(struct disk* hd) {
  struct ide_channel* channel = hd->my_channel;
  uint16_t time_limit = 30 * 1000;
  while (time_limit -= 10 >= 0) {
    if (!(inb(reg_status(channel)) & BIT_ALT_STAT_BSY)) {
      return (inb(reg_status(channel)) & BIT_ALT_STAT_DRQ);
    } else {
      mtime_sleep(10);
    }
  }
  return false;
}

/*从硬盘读取sec_cnt个扇区到buf*/
void ide_read(struct disk* hd, uint32_t lba, void* buf, uint32_t sec_cnt) {
  ASSERT(lba <= max_lba);
  ASSERT(sec_cnt > 0);
  lock_acquire(&hd->my_channel->lock);  // 一个通道同时只允许操作一个磁盘

  /*1.选择操作磁盘*/
  select_disk(hd);
  uint32_t secs_op;        // 每次操作的扇区数
  uint32_t secs_done = 0;  // 已完成的扇区数
  while (secs_done < sec_cnt) {
    if ((secs_done + 256) <= sec_cnt) {
      secs_op = 256;
    } else {
      secs_op = sec_cnt - secs_done;
    }

    /*2.写入待读入的扇区数和起始扇区号*/
    select_sector(hd, lba + secs_done, secs_op);

    /*3.执行的命令写入reg_cmd寄存器*/
    cmd_out(hd->my_channel, CMD_READ_SECTOR);

    sema_down(&hd->my_channel->disk_done);

    /*4.检测硬盘状态是否可读*/
    /*醒来后开始执行下面代码*/
    if (!busy_wait(hd)) {  // 若失败
      char error[64];
      sprintf(error, "%s read sector %d failed!!!!\n", hd->name, lba);
      PANIC(error);
    }

    /*5.把数据从硬盘缓存区读出*/
    read_from_sector(hd, (void*)((uint32_t)buf + secs_done * 512), secs_op);
    secs_done += secs_op;
  }
  lock_release(&hd->my_channel->lock);
}

/*从硬盘读取sec_cnt个扇区到buf*/
void ide_write(struct disk* hd, uint32_t lba, void* buf, uint32_t sec_cnt) {
  ASSERT(lba <= max_lba);
  ASSERT(sec_cnt > 0);
  lock_acquire(&hd->my_channel->lock);  // 一个通道同时只允许操作一个磁盘

  /*1.选择操作磁盘*/
  select_disk(hd);
  uint32_t secs_op;        // 每次操作的扇区数
  uint32_t secs_done = 0;  // 已完成的扇区数
  while (secs_done < sec_cnt) {
    if ((secs_done + 256) <= sec_cnt) {
      secs_op = 256;
    } else {
      secs_op = sec_cnt - secs_done;
    }

    /*2.写入待读入的扇区数和起始扇区号*/
    select_sector(hd, lba + secs_done, secs_op);

    /*3.执行的命令写入reg_cmd寄存器*/
    cmd_out(hd->my_channel, CMD_WRITE_SECTOR);

    /*4.检测硬盘状态是否可读*/
    if (!busy_wait(hd)) {  // 若失败
      char error[64];
      sprintf(error, "%s write sector %d failed!!!!\n", hd->name, lba);
      PANIC(error);
    }

    /*5.把数据写入硬盘缓存*/
    write_to_sector(hd, (void*)((uint32_t)buf + secs_done * 512), secs_op);
    sema_down(&hd->my_channel->disk_done);
    secs_done += secs_op;
  }
  lock_release(&hd->my_channel->lock);
}

/*硬盘中断处理程序*/
void intr_hd_handler(uint8_t irq_no) {
  ASSERT(irq_no == 0x2e || irq_no == 0x2f);
  uint8_t ch_no = irq_no - 0x2e;
  struct ide_channel* channel = &channels[ch_no];
  ASSERT(channel->irq_no == irq_no);
  if (channel->expecting_intr) {
    channel->expecting_intr = false;
    sema_up(&channel->disk_done);
    // 读取状态寄存器使硬盘控制器认为此次的中断已被处理,从而硬盘可以继续执行新的读写
    inb(reg_status(channel));
  }
}

/* 将 dst 中 len 个相邻字节交换位置后存入 buf */
static void swap_pairs_bytes(const char* dst, char* buf, uint32_t len) {
  uint8_t idx;
  for (idx = 0; idx < len; idx += 2) {
    buf[idx + 1] = *dst++;
    buf[idx] = *dst++;
  }
  buf[idx] = '\0';
}

static void identify_disk(struct disk* hd) {
  char id_info[512];
  select_disk(hd);
  cmd_out(hd->my_channel, CMD_IDENTIFY);

  sema_down(&hd->my_channel->disk_done);

  if (!busy_wait(hd)) {  // 若失败
    char error[64];
    sprintf(error, "%s identify failed!!!!!!\n", hd->name);
    PANIC(error);
  }

  read_from_sector(hd, id_info, 1);
  char buf[64];
  uint8_t sn_start = 10 * 2, sn_len = 20, md_start = 27 * 2, md_len = 40;
  swap_pairs_bytes(&id_info[sn_start], buf, sn_len);
  printk("disk %s info:\nSN: %s\n", hd->name, buf);
  memset(buf, 0, sizeof(buf));
  swap_pairs_bytes(&id_info[md_start], buf, md_len);
  printk("MODULE: %s\n", buf);
  uint32_t sectors = *(uint32_t*)&id_info[60 * 2];
  printk("SECTORS: %d\n", sectors);
  printk("CAPACITY: %dMB\n", sectors * 512 / 1024 / 1024);
}

/* 扫描硬盘 hd 中地址为 ext_lba 的扇区中的所有分区 */
static void partition_scan(struct disk* hd, uint32_t ext_lab) {
  struct boot_sector* bs =
      (struct boot_sector*)sys_malloc(sizeof(struct boot_sector));

  ide_read(hd, ext_lab, bs, 1);
  uint8_t part_idx = 0;
  struct partition_table_entry* p = bs->partition_table;

  /*遍历分区表四个分区表项*/
  while (part_idx++ < 4) {
    if (p->fs_type == 0x5) {  // 若为拓展分区
      /* 子扩展分区的 start_lba 是相对于主引导扇区中的总扩展分区地址*/
      if (ext_lba_base != 0) {
        partition_scan(hd, p->start_lba + ext_lba_base);
      } else {
        ext_lba_base = p->start_lba;
        partition_scan(hd, p->start_lba);
      }
    } else if (p->fs_type != 0) {  // 若是有效的分区类型
      if (ext_lab == 0) {
        hd->prim_parts[p_no].start_lba = ext_lab + p->start_lba;
        hd->prim_parts[p_no].sec_cnt = p->sec_cnt;
        hd->prim_parts[p_no].my_disk = hd;
        list_append(&partition_list, &hd->prim_parts[p_no].part_tag);
        sprintf(hd->prim_parts[p_no].name, "%s%d", hd->name, p_no + 1);
        p_no++;
        ASSERT(p_no < 4);
      } else {
        hd->logic_parts[l_no].start_lba = ext_lab + p->start_lba;
        hd->logic_parts[l_no].sec_cnt = p->sec_cnt;
        hd->logic_parts[l_no].my_disk = hd;
        list_append(&partition_list, &hd->logic_parts[l_no].part_tag);
        sprintf(hd->logic_parts[l_no].name, "%s%d", hd->name,
                l_no + 5);  // 逻辑分区是从5开始的
        l_no++;
        if (l_no >= 8) {  // 只支持 8 个逻辑分区,避免数组越界
          return;
        }
      }
    }
    p++;
  }
  sys_free(bs);
}

/* 打印分区信息 */
static bool partition_info(struct list_elem* pelem, int arg UNUSED) {
  struct partition* part = elem2entry(struct partition, part_tag, pelem);

  printk(" %s start_lba:0x%x, sec_cnt:0x%x\n", part->name, part->start_lba,
         part->sec_cnt);

  /* 在此处 return false 与函数本身功能无关,
   * 只是为了让主调函数 list_traversal 继续向下遍历元素 */
  return false;
}

/*硬盘初始化*/
void ide_init() {
  printk("ide_init start\n");
  uint8_t hd_cnt = *((uint8_t*)(0x475));  // 获取硬盘的数量
  ASSERT(hd_cnt > 0);
  list_init(&partition_list);
  channel_cnt = DIV_ROUND_UP(hd_cnt, 2);  // 计算需要的通道数量
  struct ide_channel* channel;
  uint8_t channel_no = 0, dev_no = 0;

  /*处理每个通道上的硬盘*/
  while (channel_no < channel_cnt) {
    channel = &channels[channel_no];
    sprintf(channel->name, "ide%d", channel_no);
    /*为每一个ide通道初始化端口基址及中断向量*/
    switch (channel_no) {
      case 0:
        channel->port_base = 0x1f0;  // prim通道
        channel->irq_no = 0x20 + 14;
        break;
      case 1:
        channel->port_base = 0x170;  // sec通道
        channel->irq_no = 0x20 + 15;
        break;
    }
    channel->expecting_intr = false;  // 未向硬盘写入指令时不期待硬盘的中断
    lock_init(&channel->lock);
    sema_init(&channel->disk_done, 0);
    register_handler(channel->irq_no, intr_hd_handler);

    while (dev_no < 2) {
      struct disk* hd = &channel->devices[dev_no];
      hd->my_channel = channel;
      hd->dev_no = dev_no;
      sprintf(hd->name, "sd%c", 'a' + channel_no * 2 + dev_no);
      identify_disk(hd);  // 获取硬盘参数
      if (dev_no != 0) {  // 主盘是裸盘，不处理
        partition_scan(hd, 0);
      }
      p_no = 0;
      l_no = 0;
      dev_no++;
    }
    dev_no = 0;  // 置零，为下一个循环使用(下一个channel)

    channel_no++;
  }
  printk("\nall partition info\n");
  list_traversal(&partition_list, partition_info, (int)NULL);

  printk("ide_init done\n");
}