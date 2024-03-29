u6

文件系统负责将逻辑上的目录树结构（包括其中每个文件或目录的数据和其他信息）映射到持久存储设备上决定设备上的每个扇区各应存储哪些内容。

inode 中不仅包含了我们通过 `stat` 工具能够看到的文件/目录的元数据（大小/访问权限/类型等信息），还包含实际保存对应文件/目录数据的数据块（位于最后的数据块区域中）的索引信息，从而能够找到文件/目录的数据被保存在磁盘的哪些块中。从索引方式上看，同时支持直接索引和间接索引。

当一个程序需要访问某个文件时，操作系统会首先读取这个文件的 inode（disk inode），然后创建一个内存中的 inode 结构体（memory inode）。内存中的 inode 其实是 disk inode 的一个副本，它保存了和磁盘上一样的文件信息，以及一些额外的元数据用于管理这个文件。每个打开文件都对应着内存中的一个 inode，操作系统会根据这个 inode 执行相应的 I/O 操作。





[ boot block | sb block | inode blocks | free bit map | data blocks ] 是文件系统的布局结构。

boot block 是启动块，用于引导操作系统启动；
sb block 是超级块，也就是文件系统的元数据块，记录文件系统的相关信息，如块数量、inode 数量等；
inode blocks 是inode块，其中储存的是文件和目录的元数据，包括文件大小，所属用户和权限等信息。
free bit map 是储存空闲块位置的位图，记录哪些块是可用的；
data blocks 是实际存放文件数据的块。

它们之间的关系是：
sb block会记录inode block和data block的地址信息，以方便系统在需要时快速定位到相应的块，而free bit map则记录哪些块是可用的，以便系统在写入文件时分配合适的块。

因此，这五个部分共同构成了一个完整的文件系统，使得系统能够有效地组织和存储文件数据，并在需要时快速定位到相应的数据块。

---

没有实现

我们的内核实现对于目录树结构进行了很大程度上的简化，这样做的目的是为了能够完整的展示文件系统的工作原理，但代码量又不至于太多。我们进行的简化如下：

- 扁平化：仅存在根目录 `/` 一个目录，剩下所有的文件都放在根目录内。在索引一个文件的时候，我们直接使用文件的文件名而不是它含有 `/` 的绝对路径。
- 权限控制：我们不设置用户和用户组概念，全程只有单用户。同时根目录和其他文件也都没有权限控制位，即完全不限制文件的访问方式，不会区分文件是否可执行。
- 不记录文件访问/修改的任何时间戳。
- 不支持软硬链接。
- 除了下面即将介绍的系统调用之外，其他的很多文件系统相关系统调用均未实现。





---

如果内核可以随时切换，当前有那些数据结构可能被破坏。提示：想想 kalloc 分配到一半，进程 switch 切换到一半之类的。

---

磁盘块的读写是通过中断处理，按照 qemu 对 virtio 的定义，实现了 virtio_disk_init 和 virtio_disk_rw 两个函数

irtio_disk_rw 实际完成磁盘IO，当设定好读写信息后会通过 MMIO 的方式通知磁盘开始写

当磁盘完成 IO 后，磁盘会触发一个外部中断，在中断处理中会把死循环条件解除。内核态只会在处理磁盘读写的时候短暂开启中断，之后会马上关闭。

---

如果内核可以随时切换，当前有那些数据结构可能被破坏。提示：想想 kalloc 分配到一半，进程 switch 切换到一半之类的。

---

通过设备号和逻辑块号如何确定一个缓存块

bmap函数是连接inode和block的重要函数。但由于我们支持了间接索引，同时还设计到文件大小的改变

block-dinode-inode-file



---



用户打开文件过程：

调用root_dir获取根目录对应的inode。之后就遍历这个inode索引的数据块中存储的文件信息到dirent结构体之中，比较名称和给定的文件名是否一致

通过file name对应的inode num去从磁盘读取对应的inode。为了解决共享问题（不同进程可以打开同一个磁盘文件），也有一个全局的 inode table，每当新打开一个文件的时候，会把一个空闲的　inode 绑定为对应 dinode 的缓存，这一步通过 iget　完成。

create:

ialloc 干的事情：遍历 inode blocks 找到一个空闲的inode，初始化并返回。dirlink对于本章的练习也十分重要。和dirlookup不同，我们没有现成的dirent存储在磁盘上，而是要在磁盘上创建一个新的dirent。他遍历根目录数据块，找到一个空的 dirent，设置 dirent = {inum, filename} 然后返回，注意这一步可能找不到空位，这时需要找一个新的数据块，并扩大 root_dir size，这是由　bmap 自动完成的。需要注意本章创建硬链接时对应inode num的处理。