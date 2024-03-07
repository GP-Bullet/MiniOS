#ifndef LIB_KERNEL_LIST
#define LIB_KERNEL_LIST

#include "global.h"

#define offset(struct_type, member) (long int)(&(((struct_type*)0)->member))

#define elem2entry(struct_type, struct_member_name, elem_ptr) \
  (struct_type*)((long int)elem_ptr - offset(struct_type, struct_member_name))

struct list_elem {
  struct list_elem* prev;  // 前驱
  struct list_elem* next;  // 后继
};

/* 链表结构,用来实现队列*/
struct list {
  struct list_elem head;
  struct list_elem tail;
};

/* 自定义函数类型 function,用于在 list_traversal 中做回调函数 */
typedef bool(function)(struct list_elem*, int arg);

void list_init(struct list*);
void list_insert_before(struct list_elem* before, struct list_elem* elem);
void list_push(struct list* plist, struct list_elem* elem);
void list_append(struct list* plist, struct list_elem* elem);
void list_remove(struct list_elem* pelem);
struct list_elem* list_pop(struct list* plist);
bool list_empty(struct list* plist);
uint32_t list_len(struct list* plist);
struct list_elem* list_traversal(struct list* plist, function func, int arg);
bool elem_find(struct list* plist, struct list_elem* obj_elem);

void print_ele(struct list* plist);
#endif /* LIB_KERNEL_LIST */
