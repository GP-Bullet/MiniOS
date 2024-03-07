#ifndef USERPROG_ELF
#define USERPROG_ELF
#include "stdint.h"

#define EI_NIDENT (16)
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* 32 位 elf 头 */
struct Elf32_Ehdr {
  unsigned char e_ident[EI_NIDENT]; /* 魔术和一些info(大小端，版本) */
  Elf32_Half e_type;    /* 文件类型(可重定位文件，可执行文件) */
  Elf32_Half e_machine; /* 该文件支持运行的硬件平台 */
  Elf32_Word e_version; /* 版本信息 */
  Elf32_Addr e_entry;   /* 加载后的虚拟地址 */
  Elf32_Off e_phoff;    /* 程序头表在文件中的偏移量*/
  Elf32_Off e_shoff;    /* 节头表在文件中的偏移量 */
  Elf32_Word e_flags;   /* 与处理器相关的标志 */
  Elf32_Half e_ehsize;  /* elf header 的字节大小 */
  Elf32_Half e_phentsize; /* 程序头表中每个条目(entry)的字节大小 */
  Elf32_Half e_phnum;     /* 程序头表中的条目数量 */
  Elf32_Half e_shentsize; /* 节头表中每个条目(entry)的字节大小 */
  Elf32_Half e_shnum;     /* 节头表中的条目数量 */
  Elf32_Half e_shstrndx;  /* string name table 在节头表中的索引 index */
};

/* 程序头表 Program header 就是段描述头 */
struct Elf32_Phdr {
  Elf32_Word p_type;   /* 该段的类型(可加载，动态链接) */
  Elf32_Off p_offset;  /* 本段在文件内的起始偏移地址 */
  Elf32_Addr p_vaddr;  /* 指明本段在内存中的起始虚拟地址 */
  Elf32_Addr p_paddr;  /* 物理地址 */
  Elf32_Word p_filesz; /* 该段在文件中的大小 */
  Elf32_Word p_memsz;  /* 该段在内存中的大小 */
  Elf32_Word p_flags;  /* 读写可执行等权限的相关标志位 */
  Elf32_Word p_align; /* 指明本段在文件和内存中的对齐方式(对齐或不对齐) */
};

/*段类型*/
enum segment_type {
  PT_NULL,     // 忽略
  PT_LOAD,     // 可加载程序段
  PT_DYNAMIC,  // 动态加载信息
  PT_INTERP,   // 动态加载器名称
  PT_NOTE,     // 一些辅助信息
  PT_SHLIB,    // 保留
  PT_PHDR      // 程序头
};

#endif /* USERPROG_ELF */
