#include "dir.h"

#include "debug.h"
#include "file.h"
#include "fs.h"
#include "ide.h"
#include "inode.h"
#include "memory.h"
#include "process.h"
#include "stdio_kernel.h"
#include "string.h"
#include "super_block.h"
struct dir root_dir;

/*打开根目录*/
void open_root_dir(struct partition* part) {
  root_dir.inode = inode_open(part, part->sb->root_inode_no);
  root_dir.dir_pos = 0;
}

// 在pdir分区下，寻找名字为name的文件或目录
bool search_dir_entry(struct partition* part, struct dir* pdir,
                      const char* name, struct dir_entry* dir_e) {
  uint32_t block_cnt = 140;  // 12个直接块+128个一级间接块=140

  uint32_t* all_blocks = (uint32_t*)sys_malloc(48 + 512);
  if (all_blocks == NULL) {
    printk("search_dir_entry: sys_malloc for all_blocks failed");
    return false;
  }
  uint32_t block_idx = 0;
  while (block_idx < 12) {
    all_blocks[block_idx] = pdir->inode->i_sectors[block_idx];
    block_idx++;
  }
  block_idx = 0;
  if (pdir->inode->i_sectors[12] != 0) {  // 若含有一级间接块
    ide_read(part->my_disk, pdir->inode->i_sectors[12], all_blocks + 12, 1);
  }
  uint8_t* buf = (uint8_t*)sys_malloc(SECTOR_SIZE);

  struct dir_entry* p_de = (struct dir_entry*)buf;

  uint32_t dir_entry_size = part->sb->dir_entry_size;
  uint32_t dir_entry_cnt =
      SECTOR_SIZE / dir_entry_size;  // 每一页目录条目的个数

  while (block_idx < block_cnt) {
    if (all_blocks[block_idx] == 0) {
      block_idx++;
      continue;
    }
    ide_read(part->my_disk, all_blocks[block_idx], buf, 1);
    uint32_t dir_entry_idx = 0;
    // 遍历该块中所有的目录项
    while (dir_entry_idx < dir_entry_cnt) {
      if (!strcmp(p_de->filename, name)) {
        // 找到了
        memcpy(dir_e, p_de, dir_entry_size);
        sys_free(buf);
        sys_free(all_blocks);
        return true;
      }
      dir_entry_idx++;
      p_de++;
    }
    block_idx++;
    p_de = (struct dir_entry*)buf;

    memset(buf, 0, SECTOR_SIZE);
  }
  sys_free(buf);
  sys_free(all_blocks);
  return false;
}

/* 在分区 part 上打开 i 结点为 inode_no 的目录并返回目录指针 */
struct dir* dir_open(struct partition* part, uint32_t inode_no) {
  struct dir* pdir = (struct dir*)sys_malloc(sizeof(struct dir));
  pdir->inode = inode_open(part, inode_no);
  pdir->dir_pos = 0;
  return pdir;
}

/*关闭目录*/
void dir_close(struct dir* dir) {
  // 根目录不允许关闭
  if (dir == &root_dir) {
    return;
  }
  inode_close(dir->inode);
  sys_free(dir);
}

/*在内存中初始化目录项 p_de */
void create_dir_entry(char* filename, uint32_t inode_no, uint8_t file_types,
                      struct dir_entry* p_de) {
  ASSERT(strlen(filename) <= MAX_FILE_NAME_LEN);

  memcpy(p_de->filename, filename, strlen(filename));
  p_de->i_no = inode_no;
  p_de->f_type = file_types;
}

/*将目录项 p_de 写入父目录 parent_dir 中,io_buf 由主调函数提供*/
bool sync_dir_entry(struct dir* parent_dir, struct dir_entry* p_de,
                    void* io_buf) {
  struct inode* dir_inode = parent_dir->inode;
  uint32_t dir_size = dir_inode->i_size;
  uint32_t dir_entry_size = cur_part->sb->dir_entry_size;

  ASSERT(dir_size % dir_entry_size == 0);  // 保证是整数个条目
  uint32_t dir_entrys_per_sec =
      (SECTOR_SIZE / dir_entry_size);  // 每一个扇区的条目个数
  int32_t block_lba = -1;

  uint8_t block_idx = 0;
  uint32_t all_blocks[140] = {0};  // all_blocks 保存目录所有的块

  while (block_idx < 12) {
    all_blocks[block_idx] = dir_inode->i_sectors[block_idx];
    block_idx++;
  }

  struct dir_entry* dir_e = (struct dir_entry*)io_buf;
  int32_t block_bitmap_idx = -1;

  block_idx = 0;
  while (block_idx < 140) {
    block_bitmap_idx = -1;
    if (all_blocks[block_idx] == 0) {
      block_lba = block_bitmap_alloc(cur_part);
      if (block_lba == -1) {
        printk("alloc block bitmap for sync_dir_entry failed\n");
        return false;
      }
      block_bitmap_idx =
          block_lba - cur_part->sb->data_start_lba;  // 在位图中的偏移量
      ASSERT(block_bitmap_idx != -1);
      bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);

      block_bitmap_idx = -1;
      if (block_idx < 12) {  // 若为直接块
        dir_inode->i_sectors[block_idx] = all_blocks[block_idx] = block_lba;
      } else if (block_idx == 12) {            // 间接块
        dir_inode->i_sectors[12] = block_lba;  // 做一级间接块
        block_lba = -1;
        block_lba = block_bitmap_alloc(cur_part);  // 再分配一个0级间接块
        if (block_lba == -1) {                     // 分配失败
          // 回滚
          block_bitmap_idx =
              dir_inode->i_sectors[12] - cur_part->sb->data_start_lba;
          bitmap_set(&cur_part->block_bitmap, block_bitmap_idx, 0);
          bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);  // 重新同步
          dir_inode->i_sectors[12] = 0;
          printk("alloc block bitmap for sync_dir_entry failed\n");
          return false;
        }
        block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
        ASSERT(block_bitmap_idx != -1);
        bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);

        all_blocks[12] = block_lba;
        /* 把新分配的第 0 个间接块地址写入一级间接块表 */
        ide_write(cur_part->my_disk, dir_inode->i_sectors[12], all_blocks + 12,
                  1);
      } else {  // 已经分配了12
        all_blocks[block_idx] = block_lba;
        ide_write(cur_part->my_disk, dir_inode->i_sectors[12], all_blocks + 12,
                  1);
      }

      /* 再将新目录项 p_de 写入新分配的间接块 */
      memset(io_buf, 0, 512);
      memcpy(io_buf, p_de, dir_entry_size);
      ide_write(cur_part->my_disk, all_blocks[block_idx], io_buf, 1);
      dir_inode->i_size += dir_entry_size;
      return true;
    }
    ide_read(cur_part->my_disk, all_blocks[block_idx], io_buf, 1);
    /*在扇区中查找空目录项*/
    uint8_t dir_entry_idx = 0;
    while (dir_entry_idx < dir_entrys_per_sec) {
      if ((dir_e + dir_entry_idx)->f_type == FT_UNKNOWN) {
        // FT_UNKNOWN 为 0,无论是初始化,或是删除文件后,
        // 都会将 f_type 置为 FT_UNKNOWN
        memcpy(dir_e + dir_entry_idx, p_de, dir_entry_size);
        ide_write(cur_part->my_disk, all_blocks[block_idx], io_buf, 1);
        dir_inode->i_size += dir_entry_size;
        return true;
      }
      dir_entry_idx++;
    }
    block_idx++;
    // 读取二级块
    if (block_idx > 12) {
      ide_read(cur_part->my_disk, dir_inode->i_sectors[12], all_blocks + 12, 1);
    }
  }
  printk("directory is full!\n");
  return false;
}

/* 把分区 part 目录 pdir 中编号为 inode_no 的目录项删除 */
bool delete_dir_entry(struct partition* part, struct dir* pgdir,
                      uint32_t inode_no, void* io_buf) {
  struct inode* dir_inode = pgdir->inode;
  uint32_t block_idx = 0, all_blocks[140] = {0};

  /*收集目录全部地址*/
  while (block_idx < 12) {
    all_blocks[block_idx] = dir_inode->i_sectors[block_idx];
    block_idx++;
  }

  if (dir_inode->i_sectors[12] != 0) {
    ide_read(part->my_disk, dir_inode->i_sectors[12], all_blocks + 12, 1);
  }

  /*目录项存储时保证不会跨扇区*/
  uint32_t dir_entry_size = part->sb->dir_entry_size;
  uint32_t dir_entry_per_sec = (SECTOR_SIZE / dir_entry_size);

  struct dir_entry* dir_e = (struct dir_entry*)io_buf;
  struct dir_entry* dir_entry_found = NULL;
  uint8_t dir_entry_idx, dir_entry_cnt;
  bool is_dir_first_block = false;

  /*遍历所有块，寻找目录项*/
  block_idx = 0;
  while (block_idx < 140) {
    is_dir_first_block = false;
    if (all_blocks[block_idx] == 0) {
      block_idx++;
      continue;
    }

    dir_entry_idx = dir_entry_cnt = 0;
    /* 读取扇区,获得目录项 */
    ide_read(part->my_disk, all_blocks[block_idx], io_buf, 1);

    /* 遍历所有的目录项,统计该扇区的目录项数量及是否有待删除的目录项 */
    while (dir_entry_idx < dir_entry_per_sec) {
      if ((dir_e + dir_entry_idx)->f_type != FT_UNKNOWN) {
        if (!strcmp((dir_e + dir_entry_idx)->filename, ".")) {
          is_dir_first_block = true;
        } else if (strcmp((dir_e + dir_entry_idx)->filename, ".") &&
                   strcmp((dir_e + dir_entry_idx)->filename, "..")) {
          dir_entry_cnt++;  // 统计此扇区内的目录项个数,用来判断删除目录项后是否回收该扇区

          if ((dir_e + dir_entry_idx)->i_no == inode_no) {
            ASSERT(dir_entry_found == NULL);
            dir_entry_found = dir_e + dir_entry_idx;
          }
        }
      }
      dir_entry_idx++;
    }
    /*若次扇区未找到该目录项，继续在下一个扇区找*/
    if (dir_entry_found == NULL) {
      block_idx++;
      continue;
    }
    /*找到目录项*/
    ASSERT(dir_entry_cnt >= 1);

    // 若该块不是第一个块，并且只有要删除的目录项一个，则将该块直接释放
    if (dir_entry_cnt == 1 && !is_dir_first_block) {
      uint32_t block_bitmap_idx =
          all_blocks[block_idx] - part->sb->data_start_lba;
      bitmap_set(&part->block_bitmap, block_bitmap_idx, 0);
      bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
      /*将块地址从数组 i_sectors 或索引表中去掉*/
      if (block_idx < 12) {
        dir_inode->i_sectors[block_idx] = 0;
      } else {
        /*先判断一级间接索引表中间接块的数量,如果仅有这 1
         * 个间接块,连同间接索引表所在的块一同回收 */
        uint32_t indirect_blocks = 0;
        uint32_t indirect_block_idx = 12;
        while (indirect_block_idx < 140) {
          if (all_blocks[indirect_block_idx] != 0) {
            indirect_blocks++;
          }
        }
        ASSERT(indirect_blocks >= 1);  // 包括当前间接块

        // 同步
        if (indirect_blocks > 1) {
          all_blocks[block_idx] = 0;
          ide_write(part->my_disk, dir_inode->i_sectors[12], all_blocks + 12,
                    1);
        } else {  // indirect_blocks =1
          block_bitmap_idx =
              dir_inode->i_sectors[12] - part->sb->data_start_lba;
          bitmap_set(&part->block_bitmap, block_bitmap_idx, 0);
          bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
          /* 将间接索引表地址清 0 */
          dir_inode->i_sectors[12] = 0;
        }
      }
    } else {
      // 仅将该目录项清空
      memset(dir_entry_found, 0, dir_entry_size);
      ide_write(part->my_disk, all_blocks[block_idx], io_buf, 1);
    }
    /* 更新 i 结点信息并同步到硬盘 */
    ASSERT(dir_inode->i_size >= dir_entry_size);
    dir_inode->i_size -= dir_entry_size;
    memset(io_buf, 0, SECTOR_SIZE * 2);
    inode_sync(part, dir_inode, io_buf);
    return true;
  }
  return false;
}

/*读取目录，成功返回1个目录项项，失败返回NULL*/
struct dir_entry* dir_read(struct dir* dir) {
  struct dir_entry* dir_e = (struct dir_entry*)dir->dir_buf;
  struct inode* dir_inode = dir->inode;
  uint32_t all_blocks[140] = {0}, block_cnt = 12;
  uint32_t block_idx = 0, dir_entry_idx = 0;

  while (block_idx < 12) {
    all_blocks[block_idx] = dir_inode->i_sectors[block_idx];
    block_idx++;
  }
  if (dir_inode->i_sectors[12] != 0) {
    // 若含有一级间接块表
    ide_read(cur_part->my_disk, dir_inode->i_sectors[12], all_blocks + 12, 1);
    block_cnt = 140;
  }
  block_idx = 0;
  uint32_t cur_dir_entry_pos = 0;

  uint32_t dir_entry_size = cur_part->sb->dir_entry_size;
  uint32_t dir_entrys_per_sec = SECTOR_SIZE / dir_entry_size;

  while (block_idx < block_cnt) {
    if (dir->dir_pos >= dir_inode->i_size) {
      return NULL;
    }
    if (all_blocks[block_idx] == 0) {
      block_idx++;
      continue;
    }
    memset(dir_e, 0, SECTOR_SIZE);
    ide_read(cur_part->my_disk, all_blocks[block_idx], dir_e, 1);

    dir_entry_idx = 0;

    /* 遍历扇区内所有目录项 */
    while (dir_entry_idx < dir_entrys_per_sec) {
      if ((dir_e + dir_entry_idx)->f_type != FT_UNKNOWN) {
        if (cur_dir_entry_pos < dir->dir_pos) {
          cur_dir_entry_pos += dir_entry_size;
          dir_entry_idx++;
          continue;
        }
        ASSERT(cur_dir_entry_pos == dir->dir_pos);
        dir->dir_pos += dir_entry_size;
        return dir_e + dir_entry_idx;
      }
      dir_entry_idx++;
    }
    block_idx++;
  }

  return NULL;
}

/*判断目录是否为空*/
bool dir_is_empty(struct dir* dir) {
  struct inode* dir_inode = dir->inode;
  /*若目录下只有.和..这两个目录项，则为空*/
  return (dir_inode->i_size == cur_part->sb->dir_entry_size * 2);
}

/*在父目录parent_dir中删除child_dir*/
int32_t dir_remove(struct dir* parent_dir, struct dir* chile_dir) {
  struct inode* child_dir_inode = chile_dir->inode;
  /* 空目录只在 inode->i_sectors[0]中有扇区,其他扇区都应该为空 */

  int32_t block_idx = 1;
  while (block_idx < 13) {
    ASSERT(child_dir_inode->i_sectors[block_idx] == 0);
    block_idx++;
  }
  void* io_buf = sys_malloc(SECTOR_SIZE * 2);
  if (io_buf == NULL) {
    printk("dir_remove: malloc for io_buf failed\n");
    return -1;
  }
  /* 在父目录 parent_dir 中删除子目录 child_dir 对应的目录项 */
  delete_dir_entry(cur_part, parent_dir, child_dir_inode->i_no, io_buf);
  /* 回收 inode 中 i_secotrs 中所占用的扇区,并同步 inode_bitmap 和 block_bitmap
   */
  inode_release(cur_part, child_dir_inode->i_no);

  sys_free(io_buf);
  return 0;
}