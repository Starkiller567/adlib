#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include "list.h"
#include "array.h"

struct item {
	list_link_t link;
	int i;
};

static int *arr = NULL;
static struct list_head list;

static void add(int i)
{
	array_add(arr, i);
	struct item *item = malloc(sizeof(*item));
	item->i = i;
	list_insert_last(&list, &item->link);
}

static void remove(int i)
{
	array_fori(arr, k) {
		if (arr[k] == i) {
			array_ordered_delete(arr, k);
			break;
		}
	}

	list_foreach_elem(&list, item, struct item, link) {
		if (item->i == i) {
			list_remove_item(&item->link);
			break;
		}
	}
}

static void check(void)
{
	int *iter;
	list_link_t *cur = list_get_head(&list);
	array_foreach(arr, iter) {
		assert(cur != &list);
		assert(*iter == container_of(cur, struct item, link)->i);
		cur = list_next(cur);
	}
}

int main()
{
	list_head_init(&list);

	srand(time(NULL));

	for (unsigned int i = 0; i < 100000; i++) {
		int r = rand() % 64;
		if (r & 1) {
			add(r);
		} else {
			remove(r);
		}
		check();
	}
}
