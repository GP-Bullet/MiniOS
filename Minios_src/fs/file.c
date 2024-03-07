#include "file.h"

#include "debug.h"
#include "dir.h"
#include "fs.h"
#include "inode.h"
#include "interrupt.h"
#include "stdio_kernel.h"
#include "string.h"
#include "super_block.h"
#include "thread.h"

/*文件表*/
struct file file_table[MAX_FILE_OPEN];

/*分配一个节点*/
int32_t inode_bitmap_alloc(struct partition* part) {
  int32_t bit_idx = bitmap_scan(&part->inode_bitmap, 1);
  if (bit_idx == -1) {
    return -1;
  }
  bitmap_set(&part->inode_bitmap, bit_idx, 1);
  return bit_idx;
}

// 分配一个扇区，返回其扇区地址
int32_t block_bitmap_alloc(struct partition* part) {
  int32_t bit_idx = bitmap_scan(&part->block_bitmap, 1);
  if (bit_idx == -1) {
    return -1;
  }
  bitmap_set(&part->block_bitmap, bit_idx, 1);
  return (part->sb->data_start_lba + bit_idx);
}

/*从文件表file_table中获取一个空闲位，成功返回下标，失败返回-1*/
int32_t get_free_slot_in_global(void) {
  uint32_t fd_idx = 3;
  while (fd_idx < MAX_FILE_OPEN) {
    if (file_table[fd_idx].fd_inode == NULL) {
      break;
    }
    fd_idx++;
  }
  if (fd_idx == MAX_FILE_OPEN) {
    printk("exceed max open files\n");
    return -1;
  }
  return fd_idx;
}

/*将内存中bitmap第bit_idx位所在的512字节同步到硬盘*/
void bitmap_sync(struct partition* part, uint32_t bit_idx, uint8_t btmp) {
  uint32_t off_sec = bit_idx / 4096;         // 该位扇区偏移量
  uint32_t off_size = off_sec * BLOCK_SIZE;  // 该位字节偏移量

  uint32_t sec_lba = 0;
  uint8_t* bitmap_off = 0;
  /*需要被同步到硬盘的位图只有inode_bitmap和block_map*/
  switch (btmp) {
    case INODE_BITMAP:
      sec_lba = part->sb->inode_bitmap_lba + off_sec;
      bitmap_off = part->inode_bitmap.bits + off_size;
      break;
    case BLOCK_BITMAP:
      sec_lba = part->sb->block_bitmap_lba + off_sec;
      bitmap_off = part->block_bitmap.bits + off_size;
      break;
  }
  ide_write(part->my_disk, sec_lba, bitmap_off, 1);
}

int32_t file_create(struct dir* parent_dir, char* filename, uint8_t flag) {
  /*后续操作的公共缓冲区*/
  void* io_buf = sys_malloc(1024);
  if (io_buf == NULL) {
    printk("in file_creat: sys_malloc for io_buf failed\n");
    return -1;
  }

  uint8_t rollback_step = 0;  // 用于回滚操作状态

  /*为新文件分配inode*/
  int32_t inode_no = inode_bitmap_alloc(cur_part);
  if (inode_no == -1) {
    printk("in file_creat: allocate inode failed\n");
    return -1;
  }
  struct task_struct* cur = running_thread();

  /* 此 inode 要从堆中申请内存,不可生成局部变量(函数退出时会释放)
   * 因为 file_table 数组中的文件描述符的 inode 指针要指向它 */
  uint32_t* cur_pagedir_bak = cur->pgdir;
  cur->pgdir = NULL;  // 暂时置为空
  struct inode* new_file_inode =
      (struct inode*)sys_malloc(sizeof(struct inode));
  if (new_file_inode == NULL) {
    printk("file_create: sys_malloc for inode failded\n");
    rollback_step = 1;
    goto rollback;
  }
  /*恢复pgdir*/
  cur->pgdir = cur_pagedir_bak;

  inode_init(inode_no, new_file_inode);  // 初始化inode节点

  int fd_idx = get_free_slot_in_global();
  if (fd_idx == -1) {
    printk("exceed max open files\n");
    rollback_step = 2;
    goto rollback;
  }

  file_table[fd_idx].fd_inode = new_file_inode;
  file_table[fd_idx].fd_pos = 0;
  file_table[fd_idx].fd_flag = flag;
  file_table[fd_idx].fd_inode->write_deny = false;
  struct dir_entry new_dir_entry;
  memset(&new_dir_entry, 0, sizeof(struct dir_entry));
  create_dir_entry(filename, inode_no, FT_REGULAR, &new_dir_entry);

  /*同步内存数据到磁盘*/
  if (!sync_dir_entry(parent_dir, &new_dir_entry, io_buf)) {
    printk("sync dir_entry to disk failed\n");
    rollback_step = 3;
    goto rollback;
  }

  memset(io_buf, 0, 1024);
  /*将父目录的i节点的内容同步到硬盘*/
  inode_sync(cur_part, parent_dir->inode, io_buf);

  memset(io_buf, 0, 1024);
  /*将新创建文件的 i 结点内容同步到硬盘*/
  inode_sync(cur_part, new_file_inode, io_buf);

  /*将inode_bitmap位图同步到硬盘*/
  bitmap_sync(cur_part, inode_no, INODE_BITMAP);

  /*将创建的文件inode节点添加到open_inodes链表*/

  list_push(&cur_part->open_inodes, &new_file_inode->inode_tag);
  new_file_inode->i_open_cnts = 1;
  sys_free(io_buf);

  return pcb_fd_install(fd_idx);
rollback:
  switch (rollback_step) {
    case 3:
      memset(&file_table[fd_idx], 0, sizeof(struct file));
    case 2:
      sys_free(new_file_inode);
    case 1:
      bitmap_set(&cur_part->inode_bitmap, inode_no, 0);
      break;
  }
  sys_free(io_buf);
  return -1;
}

int32_t file_open(uint32_t inode_no, uint8_t flag) {
  int fd_idx = get_free_slot_in_global();
  if (fd_idx == -1) {
    printk("exceed max open files\n");
    return -1;
  }
  file_table[fd_idx].fd_inode = inode_open(cur_part, inode_no);
  file_table[fd_idx].fd_pos = 0;
  file_table[fd_idx].fd_flag = flag;
  bool* write_deny = &file_table[fd_idx].fd_inode->write_deny;

  // 如果是具有写权限的打开方式，访问write_den要在临界区
  if (flag & O_RDWR || flag & O_WRONLY) {
    enum intr_status old_status = intr_disable();
    if (!(*write_deny)) {
      *write_deny = true;
      intr_set_status(old_status);
    } else {
      intr_set_status(old_status);
      printk("file can’t be open now, try again later\n");
      return -1;
    }
  }
  return pcb_fd_install(fd_idx);
}

int32_t file_close(struct file* f) {
  if (f == NULL) {
    return -1;
  }
  f->fd_inode->write_deny = false;

  inode_close(f->fd_inode);
  f->fd_inode = NULL;
  return 0;
}

/* 把 buf 中的 count 个字节写入 file*/
int32_t file_write(struct file* file, const void* buf, uint32_t count) {
  if ((file->fd_inode->i_size + count) > (BLOCK_SIZE * 140)) {
    // 文件目前最大只支持 512*140=71680 字节
    printk("exceed max file_size 71680 bytes,write file failed\n");
    return -1;
  }
  uint8_t* io_buf = sys_malloc(512);  // 充当缓冲区
  if (io_buf == NULL) {
    printk("file_write: sys_malloc for io_buf failed\n");
    return -1;
  }

  uint32_t* all_blocks = (uint32_t*)sys_malloc(140 * 4);  // 存放块索引140
  if (all_blocks == NULL) {
    printk("file_write: sys_malloc for all_blocks failed\n");
    return -1;
  }

  const uint8_t* src = buf;    // 指向带写入的缓冲区
  uint32_t bytes_written = 0;  // 用来记录已写入数据大小
  uint32_t size_left = count;  // 用来记录位写入数据的大小
  int32_t block_lba = -1;      // 块地址
  uint32_t block_bitmap_idx = 0;

  uint32_t sec_idx;              // 用来索引扇区
  uint32_t sec_lba;              // 扇区地址
  uint32_t sec_off_bytes;        // 扇区内字节偏移量
  uint32_t sec_left_bytes;       // 扇区内剩余字节量
  uint32_t chunk_size;           // 每次写入硬盘的数据块大小
  int32_t indirect_block_table;  // 用来获取一级间接表地址
  uint32_t block_idx;            // 块索引

  /*判断文件是否是第一次写，如果是，先为其分配一个块*/
  if (file->fd_inode->i_sectors[0] == 0) {
    block_lba = block_bitmap_alloc(cur_part);
    if (block_lba == -1) {
      printk("file_write: block_bitmap_alloc failed\n");
      return -1;
    }

    file->fd_inode->i_sectors[0] = block_lba;

    /* 每分配一个块就将位图同步到硬盘 */
    block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
    ASSERT(block_bitmap_idx != 0);
    bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
  }
  /* 写入count个字节前,该文件已经占用的块数 */
  uint32_t file_has_used_blocks = file->fd_inode->i_size / BLOCK_SIZE + 1;

  /* 存储count字节后该文件将占用的块数 */
  uint32_t file_will_use_blocks =
      (file->fd_inode->i_size + count) / BLOCK_SIZE + 1;

  ASSERT(file_will_use_blocks <= 140);

  // 通过此判断是否需要分配扇区
  uint32_t add_blocks = file_will_use_blocks - file_has_used_blocks;
  if (add_blocks == 0) {
    /*在同一个扇区内写入数据，不涉及到分配型的扇区*/
    if (file_will_use_blocks <= 12) {
      // 使用直接块
      block_idx = file_has_used_blocks - 1;
      // 指向最后一个已经有block_idx数据的扇区
      all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];
    } else {
      // 使用间接块
      ASSERT(file->fd_inode->i_sectors[12] != 0);
      indirect_block_table = file->fd_inode->i_sectors[12];
      ide_read(cur_part->my_disk, indirect_block_table, all_blocks + 12, 1);
    }
  } else {
    // 如果有增量，分三种情况
    if (file_will_use_blocks <= 12) {
      // 第一种情况，全使用直接块
      block_idx = file_has_used_blocks - 1;
      ASSERT(file->fd_inode->i_sectors[block_idx] != 0);
      all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];

      /*在未来要用到的扇区分配好后写入all_blocks*/
      block_idx = file_has_used_blocks;  // 指向第一个要分配的扇区
      while (block_idx < file_will_use_blocks) {
        block_lba = block_bitmap_alloc(cur_part);
        if (block_lba == -1) {
          printk("file_write: block_bitmap_alloc for situation 1 failed\n");
          return -1;
        }
        /* 写文件时,不应该存在块未使用,但已经分配扇区的情况,当文件删除时,就会把块地址清
         * 0 */
        ASSERT(file->fd_inode->i_sectors[block_idx] == 0);

        file->fd_inode->i_sectors[block_idx] = all_blocks[block_idx] =
            block_lba;

        // 进行同步
        block_bitmap_idx = block_lba - cur_part->sb->block_bitmap_lba;
        bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);

        block_idx++;  // 下一个分配的新扇区
      }
    } else if (file_has_used_blocks <= 12 && file_will_use_blocks > 12) {
      // 第二种，直接块和间接块都使用

      block_idx = file_has_used_blocks - 1;

      // 指向旧数据所在的最后一个扇区
      all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];
      /*创建一级间接块表*/
      block_lba = block_bitmap_alloc(cur_part);
      if (block_lba == -1) {
        printk("file_write: block_bitmap_alloc for situation 2 failed\n");
        return -1;
      }
      ASSERT(file->fd_inode->i_sectors[12] == 0);  // 确保一级间接块表未分配

      /* 分配一级间接块索引表 */
      indirect_block_table = file->fd_inode->i_sectors[12] = block_lba;
      // 进行同步
      block_bitmap_idx = block_lba - cur_part->sb->block_bitmap_lba;
      bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);

      block_idx = file_has_used_blocks;
      while (block_idx < file_will_use_blocks) {
        block_lba = block_bitmap_alloc(cur_part);
        if (block_lba == -1) {
          printk("file_write: block_bitmap_alloc for situation 2 failed\n");
          return -1;
        }

        if (block_idx < 12) {
          ASSERT(file->fd_inode->i_sectors[block_idx] == 0);
          file->fd_inode->i_sectors[block_idx] = all_blocks[block_idx] =
              block_lba;
        } else {
          // 间接块只写入到 all_block 数组中,待全部分配完成后一次性同步到硬盘
          all_blocks[block_idx] = block_lba;
        }
        // 进行同步
        block_bitmap_idx = block_lba - cur_part->sb->block_bitmap_lba;
        bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);

        block_idx++;  // 下一个扇区
      }
      // 同步一级间接块表到硬盘
      ide_write(cur_part->my_disk, indirect_block_table, all_blocks + 12, 1);
    } else if (file_has_used_blocks > 12) {
      // 第三种，全部使用间接块
      ASSERT(file->fd_inode->i_sectors[12] != 0);
      // 已经具备一级间接块表
      // 将所有的间接块读入内存
      indirect_block_table = file->fd_inode->i_sectors[12];
      ide_read(cur_part->my_disk, indirect_block_table, all_blocks + 12, 1);

      block_idx = file_has_used_blocks;  // 第一个未使用的间接块

      while (block_idx < file_will_use_blocks) {
        block_lba = block_bitmap_alloc(cur_part);
        if (block_lba == -1) {
          printk("file_write: block_bitmap_alloc for situation 3 failed\n");
          return -1;
        }
        all_blocks[block_idx++] = block_lba;
        /* 每分配一个块就将位图同步到硬盘 */
        block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
        bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
      }
      // 同步一级间接块表到硬盘
      ide_write(cur_part->my_disk, indirect_block_table, all_blocks + 12, 1);
    }
  }

  // 现在要用的块已经收集到all_blocks里面了
  bool first_write_block = true;  // 含有剩余空间的块标识
  file->fd_pos = file->fd_inode->i_size - 1;

  while (bytes_written < count) {  // 直接写完所有数据
    memset(io_buf, 0, BLOCK_SIZE);
    sec_idx = file->fd_inode->i_size / BLOCK_SIZE;
    sec_lba = all_blocks[sec_idx];
    sec_off_bytes = file->fd_inode->i_size % BLOCK_SIZE;
    sec_left_bytes = BLOCK_SIZE - sec_off_bytes;

    /* 判断此次写入硬盘的数据大小 */
    chunk_size = size_left < sec_left_bytes ? size_left : sec_left_bytes;
    if (first_write_block) {
      ide_read(cur_part->my_disk, sec_lba, io_buf, 1);
      first_write_block = false;
    }
    memcpy(io_buf + sec_off_bytes, src, chunk_size);
    ide_write(cur_part->my_disk, sec_lba, io_buf, 1);
    printk("file write at lba 0x%x\n", sec_lba);  // 调试,完成后去掉

    src += chunk_size;  // 将指针推移到下一个新数据
    file->fd_inode->i_size += chunk_size;  // 更新文件大小
    file->fd_pos += chunk_size;
    bytes_written += chunk_size;
    size_left -= chunk_size;
  }
  inode_sync(cur_part, file->fd_inode, io_buf);
  sys_free(all_blocks);
  sys_free(io_buf);
  return bytes_written;
}

/* 从文件 file 中读取 count 个字节写入 buf,
返回读出的字节数,若到文件尾则返回-1 */
int32_t file_read(struct file* file, void* buf, uint32_t count) {
  uint8_t* buf_dst = (uint8_t*)buf;
  uint32_t size = count;
  uint32_t size_left = size;

  /* 若要读取的字节数超过了文件可读的剩余量,就用剩余量作为待读取的字节数 */
  if ((file->fd_pos + count) > file->fd_inode->i_size) {
    size = file->fd_inode->i_size - file->fd_pos;
    size_left = size;
    if (size == 0) {  // 读到文件尾巴,则返回-1
      return -1;
    }
  }
  uint8_t* io_buf = sys_malloc(BLOCK_SIZE);
  if (io_buf == NULL) {
    printk("file_read: sys_malloc for io_buf failed\n");
    return -1;
  }
  uint32_t* all_blocks = (uint32_t*)sys_malloc(140 * 4);
  if (all_blocks == NULL) {
    printk("file_read: sys_malloc for all_blocks failed\n");
    return -1;
  }

  uint32_t block_read_start_idx = file->fd_pos / BLOCK_SIZE;  // 起始位置
  uint32_t block_read_end_idx = (file->fd_pos + size) / BLOCK_SIZE;  // 终止位置

  uint32_t read_blocks = block_read_end_idx - block_read_start_idx;

  ASSERT(block_read_start_idx < 139 && block_read_end_idx < 139);
  int32_t indirect_block_table;  // 用来获取一级间接表地址
  uint32_t block_idx;            // 获取待读取的块地址

  if (read_blocks == 0) {  // 在同一个扇区(两种情况)
    ASSERT(block_read_end_idx == block_read_start_idx);
    if (block_read_end_idx < 12) {
      // 第一种情况，直接块
      block_idx = block_read_end_idx;
      all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];
    } else {
      // 第二种情况，间接块
      indirect_block_table = file->fd_inode->i_sectors[12];
      ide_read(cur_part->my_disk, indirect_block_table, all_blocks + 12, 1);
    }
  } else {  // 要读多个块
    if (block_read_end_idx < 12) {
      // 第一种情况， 都是直接块
      block_idx = block_read_start_idx;
      while (block_idx <= block_read_end_idx) {
        all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];
        block_idx++;
      }
    } else if (block_read_start_idx < 12 && block_read_end_idx >= 12) {
      // 第二种情况， 直接块间接块都有

      // 先读入直接块
      block_idx = block_read_start_idx;
      while (block_idx < 12) {
        all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];
        block_idx++;
      }
      ASSERT(file->fd_inode->i_sectors[12] != 0);
      // 再读入间接块
      indirect_block_table = file->fd_inode->i_sectors[12];
      ide_read(cur_part->my_disk, indirect_block_table, all_blocks + 12, 1);

    } else {
      //  第三种情况， 都是间接块
      ASSERT(file->fd_inode->i_sectors[12] != 0);
      indirect_block_table = file->fd_inode->i_sectors[12];
      ide_read(cur_part->my_disk, indirect_block_table, all_blocks + 12, 1);
    }
  }

  // 需要用到的block地址已经收集完成
  uint32_t sec_idx, sec_lba, sec_off_bytes, sec_left_bytes, chunk_size;
  uint32_t bytes_read = 0;
  while (bytes_read < size) {
    sec_idx = file->fd_pos / BLOCK_SIZE;
    sec_lba = all_blocks[sec_idx];
    sec_off_bytes = file->fd_pos % BLOCK_SIZE;
    sec_left_bytes = BLOCK_SIZE - sec_off_bytes;  // 这个扇区要读的数据数量
    chunk_size = size_left < sec_left_bytes ? size_left : sec_left_bytes;

    memset(io_buf, 0, BLOCK_SIZE);

    ide_read(cur_part->my_disk, sec_lba, io_buf, 1);
    memcpy(buf_dst, io_buf + sec_off_bytes, chunk_size);
    buf_dst += chunk_size;
    file->fd_pos += chunk_size;
    bytes_read += chunk_size;
    size_left -= chunk_size;
  }
  sys_free(all_blocks);
  sys_free(io_buf);
  return bytes_read;
}