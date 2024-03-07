#ifndef FS_INODE
#define FS_INODE
#include "global.h"
#include "ide.h"
#include "list.h"
#include "stdint.h"
/*inode结构*/
struct inode {
  uint32_t i_no;    // inode编号
  uint32_t i_size;  // 文件大小(若为目录，是指目录项大小之和)
  uint32_t i_open_cnts;  // 记录此文件被打开的次数
  bool write_deny;  // 写文件不能并行，进程写文件前检查此标识
  uint32_t i_sectors[13];  // 块的索引表，前12项为直接块，后1项为间接块
  struct list_elem inode_tag;
};

void inode_sync(struct partition* part, struct inode* inode, void* io_buf);

struct inode* inode_open(struct partition* part, uint32_t inode_no);

void inode_close(struct inode* inode);
void inode_init(uint32_t inode_no, struct inode* new_inode);
void inode_delete(struct partition* part, uint32_t inode_no, void* io_buf);
void inode_release(struct partition*part,uint32_t inode_no);
#endif /* FS_INODE */
