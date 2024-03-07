;-------------loader 和 kernel----------
 LOADER_BASE_ADDR equ 0x900 ;loader加载地址
 LOADER_STACK_TOP equ LOADER_BASE_ADDR
 LOADER_START_SECTOR equ 0x2

 KERNEL_BIN_BASE_ADDR equ 0x70000  ;kernel加载地址
 KERNEL_ENTRY_POINT equ 0xc0001500
 ;KERNEL_ENTRY_POINT equ 0xc00015cd
 KERNEL_START_SECTOR equ 0x9

 PAGE_DIR_TABLE_POS equ 0x100000
;--------------   gdt描述符属性  -------------
;先描述高32位的 0~23
;G段界限粒度 0表示1字节 1表示4kb(我们选择4kb)
 DESC_G_4K equ 1_00000000000000000000000b
;D 1表示使用EIP/ESP(32位) 0表示使用IP/SP(16位)(我们选择EIP/ESP) 
 DESC_D_32 equ 1_0000000000000000000000b
;L 是否使用64位代码段(我们使用32位)
 DESC_L    equ 0_000000000000000000000b
;AVL 该位置CPU不使用，用户可字节使用(暂置为0)
 DESC_AVL  equ 0_00000000000000000000b 
;段界限
 DESC_LIMIT_CODE2 equ 1111_0000000000000000b
 DESC_LIMIT_DATA2 equ DESC_LIMIT_CODE2
 DESC_LIMIT_VIDEO2 equ 0000_000000000000000b
 ;P 表示该段是否已经加载至内存
 DESC_P equ 1_000000000000000b
 ;DPL 2位的特权级 0,1,2,3
 DESC_DPL_0 equ 00_0000000000000b
 DESC_DPL_1 equ 01_0000000000000b
 DESC_DPL_2 equ 10_0000000000000b
 DESC_DPL_3 equ 11_0000000000000b
;S S 为 0 表示系统段,S 为 1 表示非系统段(下面前两个为非系统段第三个为系统段)
 DESC_S_CODE equ 1_000000000000b
 DESC_S_DATA equ DESC_S_CODE
 DESC_S_sys  equ 0_000000000000b
;TYPE字段 
;x=1,c=0,r=0,a=0 代码段是可执行的,非一致性,不可读,已访问位 a 清 0
 DESC_TYPE_CODE equ 1000_00000000b
;x=0,e=0,w=1,a=0 数据段是不可执行的,向上扩展的,可写,已访问位 a 清 0
 DESC_TYPE_DATA equ 0010_00000000b


;GTL高32位整合
;CODE
 DESC_CODE_HIGH4 equ (0x00<<24) + DESC_G_4K + DESC_D_32 + \
 DESC_L + DESC_AVL + DESC_LIMIT_CODE2 + DESC_P + \
 DESC_DPL_0 + DESC_S_CODE + DESC_TYPE_CODE +0x00

;DATA
 DESC_DATA_HIGH4 equ (0x00<<24) + DESC_G_4K + DESC_D_32 + \
 DESC_L + DESC_AVL + DESC_LIMIT_DATA2 + DESC_P + \
 DESC_DPL_0 + DESC_S_DATA + DESC_TYPE_DATA +0x00
 
;VIDEO
 DESC_VIDEO_HIGH4 equ (0x00<<24) + DESC_G_4K + DESC_D_32 + \
 DESC_L + DESC_AVL + DESC_LIMIT_VIDEO2 + DESC_P + \
 DESC_DPL_0 + DESC_S_DATA + DESC_TYPE_DATA +0x0b



 ;--------------   选择子属性  ---------------
 ;权级RPL
 RPL0 equ 00b
 RPL1 equ 01b
 RPL2 equ 10b
 RPL3 equ 11b
 ;TI 指定是GDT(全局描述符表)还是LDT(局部描述符表)
 TI_GDT equ 000b
 TI_LDT equ 100b


 ;----------------   页表相关属性    --------------
 PG_P  equ   1b           ;表示存在于物理内存当中
 PG_RW_R	 equ  00b     ;可读不可写
 PG_RW_W	 equ  10b     ;可读可写
 PG_US_S	 equ  000b    ;User级(任意级别都可以访问)
 PG_US_U	 equ  100b    ;Supervisor级(特群级3不允许访问)


;-------------  program type 定义   --------------
 PT_NULL equ 0  ;无效程序头