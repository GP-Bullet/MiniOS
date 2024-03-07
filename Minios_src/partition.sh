#! /bin/sh
check_file() {
    if [ -e "hd80M.img" ]; then
        return 1
    else
        return 0
    fi
}

make_image() {
    dd if=/dev/zero of=hd80M.img bs=1M count=80
}

do_fdisk() {
    echo -e "x\n c\n 162\n h\n 16\n r\n
    n\n p\n 1\n 2048\n 35381\n n
    e\n 4\n \n \n n\n 38912\n 72245
    n\n 75776\n  109109\n
    n\n 112640\n 145973\n
    n\n  \n  \n
    t\n 5\n  66\n
    t\n 6\n  66\n
    t\n 7\n  66\n
    t\n 8\n  66\n w" | fdisk hd80M.img
}

check_partition_and_init() {
    check_file
    if [ $? -eq 0 ]; then
        make_image
        do_fdisk
    else
        echo "hd80M.img exist"
        echo "hd80M.img 的分区如下"
        fdisk -l hd80M.img
    fi

}

check_partition_and_init
