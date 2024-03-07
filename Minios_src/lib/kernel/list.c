#include "list.h"

#include "print.h"
#include "interrupt.h"
// 初始化
void list_init(struct list* list) {
  list->head.next = &list->tail;
  list->tail.prev = &list->head;
  list->head.prev = &list->tail;
  list->tail.next = &list->head;
}

// 在before前插入elem
void list_insert_before(struct list_elem* before, struct list_elem* elem) {
  // 关闭中断，并发安全
  enum intr_status old_status = intr_disable();
  before->prev->next = elem;
  elem->next = before;
  elem->prev = before->prev;
  before->prev = elem;
  intr_set_status(old_status);
}

// 添加元素到队伍首
void list_push(struct list* plist, struct list_elem* elem) {
  list_insert_before(plist->head.next, elem);
}

// 追加元素到队尾
void list_append(struct list* plist, struct list_elem* elem) {
  list_insert_before(&plist->tail, elem);
}

/* 使元素 pelem 脱离链表 */
void list_remove(struct list_elem* pelem) {
  enum intr_status old_status = intr_disable();
  pelem->next->prev = pelem->prev;
  pelem->prev->next = pelem->next;
  intr_set_status(old_status);
}

/* 将链表第一个元素弹出并返回,类似栈的 pop 操作 */
struct list_elem* list_pop(struct list* plist) {
  struct list_elem* elem = plist->head.next;
  list_remove(elem);
  return elem;
}

/* 从链表中查找元素 obj_elem,成功时返回 true,失败时返回 false */
bool elem_find(struct list* plist, struct list_elem* obj_elem) {
  struct list_elem* elem = plist->head.next;
  while (elem != &plist->tail) {
    if (elem == obj_elem) {
      return true;
    } else {
      elem = elem->next;
    }
  }
  return false;
}

/*判断链表是否为空*/
bool list_empty(struct list* plist) {
  return (plist->head.next == &plist->tail) ? true : false;
}

/*返回链表长度*/
uint32_t list_len(struct list* plist) {
  struct list_elem* elem = plist->head.next;
  int count = 0;
  while (elem != &plist->tail) {
    count++;
    elem = elem->next;
  }
  return count;
}

/* 把列表 plist 中的每个元素 elem 和 arg 传给回调函数 func,
arg 给 func 用来判断 elem 是否符合条件.
本函数的功能是遍历列表内所有元素,逐个判断是否有符合条件的元素。*/
struct list_elem* list_traversal(struct list* plist, function func, int arg) {
  if (list_empty(plist)) {  // 如果为空直接返回
    return NULL;
  }
  struct list_elem* elem = plist->head.next;
  while (elem != &plist->tail) {
    if (func(elem, arg)) {
      return elem;
    }
    elem = elem->next;
  }
  return NULL;
}

// 打印所有元素，用于调试
void print_ele(struct list* plist) {
  int count = list_len(plist);
  struct list_elem* elem = plist->head.next;
  put_str("elem count: ");
  put_int(count);
  put_char('\n');
  for (int i = 0; i < count; i++) {
    put_int(voidptrTouint32(elem));
    put_char('\n');
    elem = elem->next;
  }
}