#ifndef __list_include__
#define __list_include__

#include <stdbool.h>
#include <stddef.h>

#ifndef container_of
// from wikipedia, the ternary operator forces matching types on pointer and member
#define container_of(ptr, type, member)					\
	((type *)((char *)(1 ? (ptr) : &((type *)0)->member) - offsetof(type, member)))
#endif

struct list_head {
	struct list_head *prev;
	struct list_head *next;
};

typedef struct list_head list_link_t;

static inline struct list_head list_head_init(struct list_head *list)
{
	list->prev = list;
	list->next = list;
	return *list;
}

static inline bool list_empty(struct list_head *list)
{
	return list->next == list;
}

static inline list_link_t *list_next(list_link_t *item)
{
	return item->next;
}

static inline list_link_t *list_prev(list_link_t *item)
{
	return item->prev;
}

static inline list_link_t *list_get_head(struct list_head *list)
{
	return list_next(list);
}

static inline list_link_t *list_get_tail(struct list_head *list)
{
	return list_prev(list);
}

static inline void list_insert_after(list_link_t *pos, list_link_t *item)
{
	item->prev = pos;
	item->next = pos->next;
	pos->next->prev = item;
	pos->next = item;
}

static inline void list_insert_before(list_link_t *pos, list_link_t *item)
{
	item->prev = pos->prev;
	item->next = pos;
	pos->prev->next = item;
	pos->prev = item;
}

static inline void list_insert_first(struct list_head *list, list_link_t *item)
{
	list_insert_after(list, item);
}

static inline void list_insert_last(struct list_head *list, list_link_t *item)
{
	list_insert_before(list, item);
}

static inline void list_remove_item(list_link_t *item)
{
	item->prev->next = item->next;
	item->next->prev = item->prev;
	item->prev = (list_link_t *)0xdeadbeef;
	item->next = (list_link_t *)0xdeadbeef;
}

static struct list_head *list_remove_head(struct list_head *list)
{
	list_link_t *head = list_get_head(list);
	list_remove_item(head);
	return head;
}

static struct list_head *list_remove_tail(struct list_head *list)
{
	list_link_t *tail = list_get_tail(list);
	list_remove_item(tail);
	return tail;
}

#define list_foreach(list, itername)					\
	for (struct list_head *itername = (list)->next, *___itername__##next = itername->next; itername != (list); itername = ___itername__##next, ___itername__##next = itername->next)

#define list_foreach_elem(list, itername, type, member)			\
	for (type *itername = container_of((list)->next, type, member), *___itername__##next = container_of(itername->member.next, type, member); &itername->member != (list); itername = ___itername__##next, ___itername__##next = container_of(itername->member.next, type, member))

#endif
