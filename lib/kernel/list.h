#ifndef __KERNEL_LIST_H
#define __KERNEL_LIST_H
#include "stdint.h"

/* offset of a member in struct type */
#define offset(struct_type, member) ((int)(&((struct_type*)0)->member))
/* given a member ptr in a struct, return the struct ptr */
#define elem2entry(struct_type, member, elem_ptr) \
          ((struct_type*)((int)elem_ptr - offset(struct_type, member)))

struct list_elem {
  struct list_elem* prev;
  struct list_elem* next;
};

struct list {
  struct list_elem head;
  struct list_elem tail;
};

typedef bool (function) (struct list_elem*, int arg);

void list_init(struct list*);
void list_insert_before(struct list_elem* before, struct list_elem* elem);
void list_push(struct list* plist, struct list_elem* elem);
void list_append(struct list* plist, struct list_elem* elem);
void list_remove(struct list_elem* elem);
struct list_elem* list_pop(struct list_elem* plist);
uint32_t list_len(struct list* plist);
struct list_elem* list_tranversal(struct list* plist, function func, int arg);
bool elem_find(struct list* plist, struct list_elem* obj_elem);
bool list_empty(struct list* plist);
#endif
