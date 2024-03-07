#ifndef FS_FILE
#define FS_FILE
#include "fs.h"
#include "stdint.h"
/*文件结构*/
struct file {
  uint32_t fd_pos;  // 当前文件操作的偏移地址，-1表示最大偏移量
  uint32_t fd_flag;  // 文件操作标识
  struct inode* fd_inode;
};

/*标准输入输出描述符*/
enum std_fd {
  stdin_no,   // 0标准输入
  stdout_no,  // 1标准输出
  stderr_no   // 2标准错误
};

/*位图类型*/
enum bitmap_type {
  INODE_BITMAP,  // inode位图
  BLOCK_BITMAP   // 块位图
};

#define MAX_FILE_OPEN 32  // 系统可打开的最大文件数

extern struct file file_table[MAX_FILE_OPEN];

int32_t get_free_slot_in_global(void);
int32_t inode_bitmap_alloc(struct partition* part);
int32_t block_bitmap_alloc(struct partition* part);
int32_t file_create(struct dir* parent_dir, char* filename, uint8_t flag);
void bitmap_sync(struct partition* part, uint32_t bit_idx, uint8_t btmp);
int32_t file_open(uint32_t inode_no, uint8_t flag);
int32_t file_close(struct file* f);
int32_t file_write(struct file* file, const void* buf, uint32_t count);
int32_t file_read(struct file* file, void* buf, uint32_t count);
#endif /* FS_FILE */
