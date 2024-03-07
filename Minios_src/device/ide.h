#ifndef DEVICE_IDE
#define DEVICE_IDE
/*分区结构体*/
#include "bitmap.h"
#include "list.h"
#include "stdint.h"
#include "sync.h"
struct partition {
  uint32_t start_lba;          // 起始扇区
  uint32_t sec_cnt;            // 扇区数
  struct disk* my_disk;        // 分区所属硬盘
  struct list_elem part_tag;   // 用于队列中的标记
  char name[8];                // 分区名称
  struct super_block* sb;      // 超级快
  struct bitmap block_bitmap;  // 块位图
  struct bitmap inode_bitmap;  // inode节点位图
  struct list open_inodes;     // 本分区打开的inode节点队列
};

/*硬盘结构*/
struct disk {
  char name[8];                    // 本硬盘的名称
  struct ide_channel* my_channel;  // 此块硬盘归属的ide通道
  uint8_t dev_no;                  // 本硬盘是主还是从
  struct partition prim_parts[4];  // 主分区(最多四个)
  struct partition logic_parts[8];  // 逻辑分区可以支持无数个(此处支持8个)
};

/*ata通道*/
struct ide_channel {
  char name[8];                // 本ata通道名称
  uint16_t port_base;          // 本通道其实端口号
  uint8_t irq_no;              // 本通道所用的中断号
  struct lock lock;            // 通道锁
  bool expecting_intr;         // 表示等待硬盘的中断
  struct semaphore disk_done;  // 用于阻塞，唤醒驱动程序
  struct disk devices[2];  // 一个通道上连接两个磁盘，一主一从
};

extern uint8_t channel_cnt;
extern struct ide_channel channels[];
extern struct list partition_list;

void ide_init();
void ide_read(struct disk* hd, uint32_t lba, void* buf, uint32_t sec_cnt);
void ide_write(struct disk* hd, uint32_t lba, void* buf, uint32_t sec_cnt);

#endif /* DEVICE_IDE */
