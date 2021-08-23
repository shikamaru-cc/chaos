#include "list.h"
#include "interrupt.h"
#include "stdbool.h"
#include "stdnull.h"
#include "string.h"

void list_init(struct list* list) {
  list->head.next = &list->tail;
  list->head.prev = NULL;
  list->tail.prev = &list->head;
  list->tail.next = NULL;
}

void list_insert_before(struct list_elem* before, struct list_elem* elem) {
  enum intr_status old_status = intr_disable();

  elem->prev = before->prev;
  elem->next = before;
  before->prev->next = elem;
  before->prev = elem;

  intr_set_status(old_status);
}

void list_push(struct list* plist, struct list_elem* elem) {
  list_insert_before(plist->head.next, elem);
}

void list_iterate(struct list* plist, list_iter_fn fn) {
  struct list_elem* elem = plist->head.next;
  while (elem != &plist->tail) {
    fn(elem);
    elem = elem->next;
  }
  return;
}

void list_append(struct list* plist, struct list_elem* elem) {
  list_insert_before(&plist->tail, elem);
}

void list_remove(struct list_elem* elem) {
  enum intr_status old_status = intr_disable();

  elem->prev->next = elem->next;
  elem->next->prev = elem->prev;

  intr_set_status(old_status);
}

struct list_elem* list_top(struct list* plist) {
  return plist->head.next;
}

struct list_elem* list_pop(struct list* plist) {
  struct list_elem* pelem = plist->head.next;
  list_remove(pelem);
  return pelem;
}

uint32_t list_len(struct list* plist) {
  struct list_elem* elem = plist->head.next;
  uint32_t i = 0;
  while (elem != &plist->tail) {
    i++;
    elem = elem->next;
  }
  return i;
}

struct list_elem* list_tranversal(struct list* plist, function func, int arg) {
  struct list_elem* elem = plist->head.next;
  while (elem != &plist->tail) {
    if (func(elem, arg)) {
      return elem;
    }
    elem = elem->next;
  }
  return NULL;
}

bool elem_find(struct list* plist, struct list_elem* obj_elem) {
  struct list_elem* elem = plist->head.next;
  while (elem != &plist->tail) {
    if (elem == obj_elem) {
      return true;
    }
    elem = elem->next;
  }
  return false;
}

bool list_empty(struct list* plist) {
  return (plist->head.next == &plist->tail) ? true : false;
}
