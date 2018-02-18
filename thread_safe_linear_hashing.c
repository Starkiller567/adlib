#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "locking.c"

#define THRESHOLD 8

#define hashtable_get(key, table) __hashtable_get((void *)key, table)
#define hashtable_remove(key, table) __hashtable_remove((void *)key, table)
#define hashtable_insert(key, data, table) \
	__hashtable_insert((void *)key, (void *)data, table)

typedef unsigned long (*hashfunc_t)(const void *key);
typedef bool (*matchfunc_t)(const void *key1, const void *key2);
typedef bool (*iterfunc_t)(const void *key, void **data, void *iter_data);

struct item {
	/* TODO test whether it's faster to save the hash aswell (for splitting)*/
	void *key;
	void *data;
};

struct bucket {
	struct bucket  *next;
	unsigned long	num_items;
	struct item	items[];
};

struct bucket_slot {
	/* TODO try embedding the first block */
	struct bucket *first;
	struct rwlock  rwlock;
};

struct hashtable {
	struct ilock		ilock;
	hashfunc_t	hashfunc; /* function to compute hash from key */
	matchfunc_t	matchfunc; /* function to compare to keys */
	/* these must only be changed under a write lock on the table */
	unsigned long		n; /* number of buckets - p */
	unsigned long		p; /* index of next bucket to be split (<n) */

	unsigned long	bucketsize; /* constant */
	/* changes to this need to be atomic (or under write lock) */
	volatile unsigned long	num_items;

	struct bucket_slot     *slots;
};

/* djb2 */
static unsigned long string_hashfunc(const void *key)
{
	const char *str = key;
	unsigned long hash = 5381;
	int c;

	while ((c = *str++))
		hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

	return hash;
}

static bool string_matchfunc(const void *key1, const void *key2)
{
	return strcmp(key1, key2) == 0;
}

static unsigned long default_hashfunc(const void *key)
{
	return (unsigned long)key;
}

static bool default_matchfunc(const void *key1, const void *key2)
{
	return key1 == key2;
}

/* caller has to atleast hold an intention read lock on the table */
static unsigned long get_idx(void *key, struct hashtable *table)
{
	unsigned long hash, idx1, idx2;

	hash = table->hashfunc(key);
	idx1 = hash % table->n;
	if (idx1 >= table->p)
		return idx1;

	idx2 = hash % (table->n * 2);
	return idx2;
}

/* caller has to atleast hold an intention read lock on the table */
static inline struct bucket_slot *get_slot(void *key, struct hashtable *table)
{
	return &table->slots[get_idx(key, table)];
}

static inline struct bucket *alloc_bucket(struct hashtable *table)
{
	size_t size = sizeof(struct bucket) + table->bucketsize * sizeof(struct item);

	return calloc(1, size);
}

/*
 * Caller has to hold atleast an intention write lock on the table
 * and a write lock on the slot.
 */
static int insert_internal(void *key, void *data, struct bucket_slot *slot,
			   struct hashtable *table, bool check_duplicate)
{
	struct bucket *bucket, **indirect;
	struct item *item;
	unsigned long i;
	int retval = 0;

	indirect = &slot->first;
	bucket = slot->first;
	while (bucket) {
		if (check_duplicate) {
			for (i = 0; i < bucket->num_items; i++) {
				item = &bucket->items[i];
				if (table->matchfunc(key, item->key))
					return EEXIST;
			}
		}

		if (bucket->num_items < table->bucketsize)
			goto found_bucket;

		indirect = &bucket->next;
		bucket = bucket->next;
	}
	/* we didn't find an empty spot, allocate one */
	bucket = alloc_bucket(table);
	if (!bucket)
		return ENOMEM;

	*indirect = bucket;

found_bucket:
	item = &bucket->items[bucket->num_items++];
	item->key = key;
	item->data = data;
	return 0;
}

/*
 * Caller has to hold an intention write lock on the table
 * and write locks on the bucket slot to be split and the new
 * bucket slot we split into.
 */
static int split_bucket_slot(struct bucket_slot *slot_to_split, unsigned long old_idx,
			     struct bucket_slot *new_slot, struct hashtable *table)
{
	struct bucket_slot *slot;
	struct bucket *bucket, *insert_bucket, *next;
	struct bucket **indirect, **insert_indirect;
	struct item *item, *new_item_slot;
	unsigned long i, insert_idx, new_idx, num_items;

	indirect = &slot_to_split->first;
	bucket = slot_to_split->first;

	/* info about where to insert the next item in the old bucket */
	insert_indirect = indirect;
	insert_bucket = bucket;
	insert_idx = 0;

	while (bucket) {
		num_items = bucket->num_items;
		for (i = 0; i < num_items; i++) {
			item = &bucket->items[i];
			bucket->num_items--;
			new_idx = get_idx(item->key, table);
			if (new_idx == old_idx) {
				new_item_slot = &insert_bucket->items[insert_idx];
				if (item != new_item_slot) {
					*new_item_slot = *item;
					item->key = NULL;
					item->data = NULL;
				}
				insert_bucket->num_items++;

				insert_idx++;
				if (insert_idx == table->bucketsize) {
					insert_indirect = &insert_bucket->next;
					insert_bucket = insert_bucket->next;
					insert_idx = 0;
				}
			} else {
				/*
				 * This can only fail with ENOMEM here,
				 * we can't currently recover from that.
				 */
				if (insert_internal(item->key, item->data,
						    new_slot, table, false) != 0)
					return -ENOMEM;
				item->key = NULL;
				item->data = NULL;
			}
		}
		indirect = &bucket->next;
		bucket = bucket->next;
	}

	if (insert_idx != 0) {
		insert_indirect = &insert_bucket->next;
		insert_bucket = insert_bucket->next;
	}
	*insert_indirect = NULL;
	while (insert_bucket) {
		next = insert_bucket->next;
		free(insert_bucket);
		insert_bucket = next;
	}

	return 0;
}

/*
 * Caller has to hold a write lock on the table,
 * which will get DOWNGRADED to an intention write lock!
 */
static int grow_hashtable_internal(struct hashtable *table)
{
	struct bucket_slot *slot_to_split, *new_slot;
	unsigned long num_slots, old_p;
	size_t new_size;
	int retval;

	num_slots = table->n + table->p;
	/* maybe pre-allocate more? */
	new_size = (num_slots + 1) * sizeof(*new_slot);
	table->slots = realloc(table->slots, new_size);
	if (!table->slots)
		return ENOMEM;

	old_p = table->p++;
	if (table->p == table->n) {
		table->n *= 2;
		table->p = 0;
	}

	slot_to_split = &table->slots[old_p];
	new_slot = &table->slots[num_slots];
	memset(new_slot, 0, sizeof(*new_slot));

	rwlock_write_lock(&slot_to_split->rwlock);
	rwlock_write_lock(&new_slot->rwlock);
	ilock_downgrade_write_to_intention_write(&table->ilock);

	retval = split_bucket_slot(slot_to_split, old_p, new_slot, table);

	rwlock_unlock(&slot_to_split->rwlock);
	rwlock_unlock(&new_slot->rwlock);
	return retval;
}

int grow_hashtable(struct hashtable *table)
{
	int retval;
	ilock_write_lock(&table->ilock);
	retval = grow_hashtable_internal(table); /* lock downgrade! */
	ilock_intention_write_unlock(&table->ilock);
	return retval;
}

/* caller has to hold an intention write lock on the table */
static int maybe_grow_table(struct hashtable *table)
{
	unsigned long base_capacity, n, p;

	base_capacity = (table->n + table->p) * table->bucketsize;
	if (table->num_items * 10 > THRESHOLD * base_capacity) {
		ilock_intention_write_unlock(&table->ilock);
		ilock_write_lock(&table->ilock);
		/*
		 * We first check under an intention write lock, so someone
		 * might have already grown the hashtable while we upgraded
		 * our lock. In that case just downgrade again.
		 */
		n = *(volatile unsigned long *)&table->n;
		p = *(volatile unsigned long *)&table->p;
		base_capacity = (n + p) * table->bucketsize;
		if (table->num_items * 10 > THRESHOLD * base_capacity)
			return grow_hashtable_internal(table); /* lock downgrade! */
		else
			ilock_downgrade_write_to_intention_write(&table->ilock);
	}
	return 0;
}

int __hashtable_insert(void *key, void *data, struct hashtable *table)
{
	struct bucket_slot *slot;
	int retval;

	ilock_intention_write_lock(&table->ilock);
	__sync_fetch_and_add(&table->num_items, 1);
	/* grow before inserting, so we don't move the new item right after inserting */
	retval = maybe_grow_table(table);
	if (retval != 0)
		goto error;

	slot = get_slot(key, table);
	rwlock_write_lock(&slot->rwlock);
	retval = insert_internal(key, data, slot, table, true);
	rwlock_unlock(&slot->rwlock);
	if (retval != 0)
		goto error;

	ilock_intention_write_unlock(&table->ilock);
	return 0;

error:
	__sync_fetch_and_sub(&table->num_items, 1);
	ilock_intention_write_unlock(&table->ilock);
	return retval;
}

void *__hashtable_get(void *key, struct hashtable *table)
{
	struct bucket_slot *slot;
	struct bucket *bucket;
	struct item *item;
	unsigned long i;
	void *data;

	ilock_intention_read_lock(&table->ilock);
	slot = get_slot(key, table);
	rwlock_read_lock(&slot->rwlock);
	bucket = slot->first;
	while (bucket) {
		for (i = 0; i < bucket->num_items; i++) {
			item = &bucket->items[i];

			if (table->matchfunc(key, item->key)) {
				data = item->data;
				rwlock_unlock(&slot->rwlock);
				ilock_intention_read_unlock(&table->ilock);
				return data;
			}
		}
		bucket = bucket->next;
	}

	rwlock_unlock(&slot->rwlock);
	ilock_intention_read_unlock(&table->ilock);
	/* TODO we can't return NULL if we accept NULL as data! */
	return NULL;
}

void hashtable_iterate_shared(iterfunc_t f, void *iter_data, struct hashtable *table)
{
	struct bucket_slot *slot;
	struct bucket *bucket, *next;
	struct item *item;
	unsigned long i, k, num_slots;

	ilock_read_lock(&table->ilock);
	num_slots = table->n + table->p;
	for (i = 0; i < num_slots; i++) {
		slot = &table->slots[i];
		bucket = slot->first;
		while (bucket) {
			for (k = 0; k < bucket->num_items; k++) {
				item = &bucket->items[k];

				if (!f(item->key, &item->data, iter_data)) {
					ilock_read_unlock(&table->ilock);
					return;
				}
			}
			bucket = bucket->next;
		}
	}
	ilock_read_unlock(&table->ilock);
}

void hashtable_iterate(iterfunc_t f, void *iter_data, struct hashtable *table)
{
	struct bucket_slot *slot;
	struct bucket *bucket, *next;
	struct item *item;
	unsigned long i, k, num_slots;

	ilock_read_and_intention_write_lock(&table->ilock);
	num_slots = table->n + table->p;
	for (i = 0; i < num_slots; i++) {
		slot = &table->slots[i];
		rwlock_write_lock(&slot->rwlock);
		bucket = slot->first;
		while (bucket) {
			for (k = 0; k < bucket->num_items; k++) {
				item = &bucket->items[k];

				if (!f(item->key, &item->data, iter_data)) {
					rwlock_unlock(&slot->rwlock);
					ilock_read_and_intention_write_lock(&table->ilock);
					return;
				}
			}
			bucket = bucket->next;
		}
		rwlock_unlock(&slot->rwlock);
	}
	ilock_read_and_intention_write_unlock(&table->ilock);
}

void *__hashtable_remove(void *key, struct hashtable *table)
{
	struct bucket_slot *slot;
	struct bucket *bucket, **indirect;
	struct item *item, *last;
	void *data;
	unsigned long i;

	ilock_intention_write_lock(&table->ilock);
	slot = get_slot(key, table);
	rwlock_write_lock(&slot->rwlock);
	bucket = slot->first;
	indirect = &slot->first;
	while (bucket) {
		for (i = 0; i < bucket->num_items; i++) {
			item = &bucket->items[i];
			if (!table->matchfunc(key, item->key))
				continue;

			data = item->data;
			while (bucket->next) {
				indirect = &bucket->next;
				bucket = bucket->next;
			}
			last = &bucket->items[--bucket->num_items];
			*item = *last;
			if (bucket->num_items == 0) {
				*indirect = NULL;
				free(bucket);
			}
			rwlock_unlock(&slot->rwlock);
			__sync_fetch_and_sub(&table->num_items, 1);
			ilock_intention_write_unlock(&table->ilock);
			return data;
		}
		indirect = &bucket->next;
		bucket = bucket->next;
	}

	rwlock_unlock(&slot->rwlock);
	ilock_intention_write_unlock(&table->ilock);
	/* TODO we can't return NULL if we accept NULL as data! */
	return NULL;
}

/* caller has to hold write lock on the table */
static void clear_hashtable_internal(struct hashtable *table)
{
	struct bucket *bucket, *next;
	unsigned long i, num_slots = table->n + table->p;

	for (i = 0; i < num_slots; i++) {
		bucket = table->slots[i].first;
		table->slots[i].first = NULL;
		while (bucket) {
			next = bucket->next;
			free(bucket);
			bucket = next;
		}
	}
	table->num_items = 0;
}

void clear_hashtable(struct hashtable *table)
{
	ilock_write_lock(&table->ilock);
	clear_hashtable_internal(table);
	ilock_write_unlock(&table->ilock);
}

void destroy_hashtable(struct hashtable *table)
{
	ilock_write_lock(&table->ilock);
	clear_hashtable_internal(table);
	free(table->slots);
	free(table);
}

struct hashtable *make_hashtable(unsigned long num_buckets, unsigned long bucketsize,
				 hashfunc_t hashfunc, matchfunc_t matchfunc)
{
	struct hashtable *table;

	table = calloc(1, sizeof(*table));
	if (!table)
		return NULL;

	if (hashfunc)
		table->hashfunc = hashfunc;
	else
		table->hashfunc = default_hashfunc;
	if (matchfunc)
		table->matchfunc = matchfunc;
	else
		table->matchfunc = default_matchfunc;

	table->n	  = num_buckets;
	table->p	  = 0;
	table->bucketsize = bucketsize;
	table->num_items  = 0;

	table->slots = calloc(num_buckets, sizeof(struct bucket_slot));
	if (!table->slots) {
		free(table);
		return NULL;
	}

	return table;
}

struct hashtable *make_string_hashtable(unsigned long num_buckets,
					unsigned long bucketsize)
{
	return make_hashtable(num_buckets, bucketsize,
			      string_hashfunc, string_matchfunc);
}


static void debug_print_item(struct item *item)
{
	printf("\tkey: %p\n", item->key);
	printf("\tdata: %p\n", item->data);
}

static void debug_print_bucket_slot(struct bucket_slot *slot, struct hashtable *table)
{
	struct bucket *bucket;
	struct item *item;
	unsigned long i;

	if (!slot->first) {
		printf("(empty)\n\n");
		return;
	}

	bucket = slot->first;
	for (;;) {
		for (int i = 0; i < bucket->num_items; i++) {
			item = &bucket->items[i];
			debug_print_item(item);
		}

		bucket = bucket->next;
		if (!bucket)
			break;

		printf("\t\t|\n");
		printf("\t\t|\n");
		printf("\t\tV\n");
	}
}

static void debug_print_table(struct hashtable *table)
{
	unsigned long i, num_buckets;

	ilock_read_lock(&table->ilock);
	num_buckets = table->n + table->p;
	for (i = 0; i < num_buckets; i++) {
		if (i == table->p)
			printf("bucket %lu: P\n", i);
		else
			printf("bucket %lu:\n", i);

		debug_print_bucket_slot(&table->slots[i], table);
	}
	ilock_read_unlock(&table->ilock);
}

#include <x86intrin.h>
#include <pthread.h>
#include <time.h>

static bool increment(const void *key, void **data, void *iter_data)
{
	assert(key == *data);
	(*data)++;
	return true;
}

#define NUM_THREADS 10
static struct hashtable *table;

void *thread_main1(void *arg)
{
	unsigned long start, end, i, num_items, t = (unsigned long)arg;

	start = t * (100000 / NUM_THREADS);
	end = start + (100000 / NUM_THREADS);
	for (i = start; i < end; i++) {
		hashtable_insert(i, i, table);
	}
	for (i = start; i < end; i++) {
		hashtable_get(i, table);
	}
	for (i = start; i < end; i++) {
		hashtable_remove(i, table);
	}
}

void *debug_thread_main(void *arg)
{
	for (;;) {
		sleep(1);
		printf("num_readers %u\n", table->ilock.num_readers);
		printf("num_ireaders %u\n", table->ilock.num_i_readers);
		printf("num_iwriters %u\n", table->ilock.num_i_writers);
		printf("cur %u\n", table->ilock.futex.cur);
		printf("next_ticket %u\n", table->ilock.futex.next_ticket);
	}
}

int main(void)
{
	pthread_t debug_thread;
	pthread_t threads[NUM_THREADS];
	unsigned long i;
	unsigned long long time;

	table = make_hashtable(32, 128, NULL, NULL);

	time = __rdtsc();
	for (i = 0; i < NUM_THREADS; i++) {
		pthread_create(&threads[i], NULL, thread_main1, (void *)i);
	}
	// pthread_create(&debug_thread, NULL, debug_thread_main, NULL);

	for (i = 0; i < NUM_THREADS; i++) {
		pthread_join(threads[i], NULL);
	}
	printf("cycles elapsed: %llu\n", __rdtsc() - time);

	destroy_hashtable(table);
}
