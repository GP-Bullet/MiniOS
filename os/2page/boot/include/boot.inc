LOADER_BASE_ADDR equ 0x900   ;将loader选为0900h，为了给内核留大一点空间，注意最上面是7c00的栈
LOADER_START_SECTOR equ 0x2   ;2扇区

LOADER_STACK_TOP equ LOADER_BASE_ADDR 

PAGE_DIRTABLE  equ 0x100000   ;页目录表在内存中的起始位置——从1M开始的位置

KERNEL_BIN_BASE_ADDR equ 0x70000        ;定义内核在内存中的缓冲区，也就是将编译好的内核文件暂时存储在内存中的位置
KERNEL_START_SECTOR equ 0x9             ;定义内核在磁盘的起始扇区
KERNEL_ENTRY_POINT equ 0xc001500        ;定义内核可执行代码的入口地址




;--gdt段描述符属性 --
DESC_G_4K   equ	  1_00000000000000000000000b   
DESC_D_32   equ	   1_0000000000000000000000b     ;32位操作数
DESC_L	    equ	    0_000000000000000000000b	;  64位代码标记，此处标记为0便可。
DESC_AVL    equ	     0_00000000000000000000b	;  cpu不用此位，暂置为0,用于自定义  
DESC_LIMIT_CODE2  equ 1111_0000000000000000b
DESC_LIMIT_DATA2  equ DESC_LIMIT_CODE2
DESC_LIMIT_VIDEO2  equ 0000_000000000000000b
DESC_P	    equ		  1_000000000000000b        ;这里直接设置为1，不考虑内存段的换出段，平坦模型下通过分页来将内存换入换出
DESC_DPL_0  equ		   00_0000000000000b
DESC_DPL_1  equ		   01_0000000000000b
DESC_DPL_2  equ		   10_0000000000000b
DESC_DPL_3  equ		   11_0000000000000b
DESC_S_CODE equ		     1_000000000000b           ;都属于非系统段
DESC_S_DATA equ	  DESC_S_CODE
DESC_S_sys  equ		     0_000000000000b
DESC_TYPE_CODE  equ	      1000_00000000b	;x=1,c=0,r=0,a=0 代码段是可执行的,非依从的,不可读的,已访问位a清0.  
DESC_TYPE_DATA  equ	      0010_00000000b	;x=0,e=0,w=1,a=0 数据段是不可执行的,向上扩展的,可写的,已访问位a清0.

DESC_CODE_HIGH4 equ (0x00 << 24) + DESC_G_4K + DESC_D_32 + DESC_L + DESC_AVL + DESC_LIMIT_CODE2 + DESC_P + DESC_DPL_0 + DESC_S_CODE + DESC_TYPE_CODE + 0x00
DESC_DATA_HIGH4 equ (0x00 << 24) + DESC_G_4K + DESC_D_32 + DESC_L + DESC_AVL + DESC_LIMIT_DATA2 + DESC_P + DESC_DPL_0 + DESC_S_DATA + DESC_TYPE_DATA + 0x00
DESC_VIDEO_HIGH4 equ (0x00 << 24) + DESC_G_4K + DESC_D_32 + DESC_L + DESC_AVL + DESC_LIMIT_VIDEO2 + DESC_P + DESC_DPL_0 + DESC_S_DATA + DESC_TYPE_DATA + 0x0b


;选择子
RPL0 equ 00b 
RPL1 equ 01b 
RPL2 equ 10b 
RPL3 equ 11b 
TI_GDT equ 0_00b 
TI_LDT equ 1_00b 


;模块化的页目录表字段,PWT PCD A D G AVL 暂时不用设置   ----------
PG_P  equ   1b
PG_RW_R	 equ  00b 
PG_RW_W	 equ  10b 
PG_US_S	 equ  000b 
PG_US_U	 equ  100b  




 ;-------------  程序段的 type 定义   --------------
PT_NULL equ 0