/* TODO use a bitmask for sys_futex to not always wake up all thread */

#include <x86intrin.h>
#include <unistd.h>
#include <limits.h>
#include <linux/futex.h>
#include <sys/time.h>
#include <sys/syscall.h>

#include <stdio.h>

#define MBARRIER asm volatile("": : :"memory")

static int sys_futex(int *uaddr, int futex_op, int val,
		     const struct timespec *timeout, int *uaddr2, int val3)
{
	return syscall(SYS_futex, uaddr, futex_op, val,
		       timeout, uaddr, val3);
}

static void wait_on_ticket(volatile int *uaddr, int val, unsigned int ticket)
{
	int bitset = 1 << (ticket % (sizeof(int) * 8 - 1));

	sys_futex((int *)uaddr, FUTEX_WAIT_BITSET_PRIVATE, val, NULL, NULL, bitset);
}

static void wake_by_ticket(volatile int *uaddr, unsigned int ticket)
{
	int bitset = 1 << (ticket % (sizeof(int) * 8 - 1));

	sys_futex((int *)uaddr, FUTEX_WAKE_BITSET_PRIVATE, INT_MAX, NULL, NULL, bitset);
}

struct futex {
	volatile unsigned int cur;
	volatile unsigned int next_ticket;
};

void futex_init(struct futex *lock)
{
	lock->cur = 0;
	lock->next_ticket = 0;
}

void futex_spinlock(struct futex *lock)
{
	unsigned int ticket = __sync_fetch_and_add(&lock->next_ticket, 1);

	while (ticket != lock->cur)
		_mm_pause();
}

void futex_lock(struct futex *lock)
{
	unsigned int i, cur, ticket = __sync_fetch_and_add(&lock->next_ticket, 1);

	for (i = 0; i < 100; i++) {
		if (ticket == lock->cur)
			return;
		_mm_pause();
	}

	cur = lock->cur;
	MBARRIER;
	while (ticket != cur) {
		wait_on_ticket(&lock->cur, cur, ticket);
		MBARRIER;
		cur = lock->cur;
		MBARRIER;
	}
}

void futex_unlock(struct futex *lock)
{
	lock->cur++;
	MBARRIER;
	wake_by_ticket(&lock->cur, lock->cur);
}

struct spinlock {
	volatile unsigned int cur;
	volatile unsigned int next_ticket;
};

void spinlock_init(struct spinlock *lock)
{
	lock->cur = 0;
	lock->next_ticket = 0;
}

void spinlock_lock(struct spinlock *lock)
{
	unsigned int ticket = __sync_fetch_and_add(&lock->next_ticket, 1);

	while (ticket != lock->cur)
		_mm_pause();
}

void spinlock_unlock(struct spinlock *lock)
{
	lock->cur++;
	MBARRIER;
}

struct rwlock {
	volatile unsigned int cur;
	volatile unsigned int next_ticket;
	volatile int num_readers; /* -1 if a writer holds the lock */
};

void rwlock_init(struct rwlock *lock)
{
	lock->cur = 0;
	lock->next_ticket = 0;
	lock->num_readers = 0;
}

void rwlock_read_spinlock(struct rwlock *lock)
{
	unsigned int ticket = __sync_fetch_and_add(&lock->next_ticket, 1);
	while (ticket != lock->cur)
		_mm_pause();

	__sync_fetch_and_add(&lock->num_readers, 1);

	__sync_fetch_and_add(&lock->cur, 1);
}

void rwlock_write_spinlock(struct rwlock *lock)
{
	unsigned int ticket = __sync_fetch_and_add(&lock->next_ticket, 1);


	while (ticket != lock->cur || lock->num_readers != 0)
		_mm_pause();
}

void rwlock_read_lock(struct rwlock *lock)
{
	unsigned int num_readers, ticket = __sync_fetch_and_add(&lock->next_ticket, 1);

	num_readers = lock->num_readers;
	MBARRIER;
	while (ticket != lock->cur) {
		sys_futex((int *)&lock->num_readers, FUTEX_WAIT_PRIVATE, num_readers, NULL, NULL, 0);
		MBARRIER;
		num_readers = lock->num_readers;
		MBARRIER;
	}

	__sync_fetch_and_add(&lock->num_readers, 1);

	__sync_fetch_and_add(&lock->cur, 1);
}

void rwlock_write_lock(struct rwlock *lock)
{
	unsigned int num_readers, ticket = __sync_fetch_and_add(&lock->next_ticket, 1);

	num_readers = lock->num_readers;
	MBARRIER;
	while (num_readers != 0 || ticket != lock->cur) {
		sys_futex((int *)&lock->num_readers, FUTEX_WAIT_PRIVATE, num_readers, NULL, NULL, 0);
		MBARRIER;
		num_readers = lock->num_readers;
		MBARRIER;
	}

	lock->num_readers--;
}

void rwlock_unlock(struct rwlock *lock)
{
	if (lock->num_readers == -1) {
		lock->num_readers++;
		lock->cur++;
	} else {
		__sync_fetch_and_add(&lock->num_readers, -1);
	}

	sys_futex((int *)&lock->num_readers, FUTEX_WAKE_PRIVATE, INT_MAX, NULL, NULL, 0);
	// wake_by_ticket(&lock->cur, lock->cur);
}

#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <assert.h>

static unsigned long long total_time = 0;
static volatile unsigned long sum = 0;
static struct spinlock spinlock;
static struct futex futex;
static struct rwlock rwlock;

#define BILLION 1000000000L

void *thread_main_writer(void *arg)
{
	struct timespec start, end;
	unsigned long long diff;
	usleep(1000);
	clock_gettime(CLOCK_MONOTONIC, &start);

	// rwlock_write_spinlock(&rwlock);
	futex_lock(&futex);
	MBARRIER;
	sum += (unsigned long)arg;
	MBARRIER;
	sum += (unsigned long)arg;
	MBARRIER;
	sum -= (unsigned long)arg;
	MBARRIER;
	sum -= (unsigned long)arg;
	MBARRIER;
	sum += (unsigned long)arg;
	MBARRIER;
	sum -= (unsigned long)arg;
	MBARRIER;
	sum += (unsigned long)arg;
	MBARRIER;
	sum -= (unsigned long)arg;
	MBARRIER;
	// rwlock_unlock(&rwlock);
	futex_unlock(&futex);

	clock_gettime(CLOCK_MONOTONIC, &end);
	diff = BILLION * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
	__sync_fetch_and_add(&total_time, diff);
}

void *thread_main_reader(void *arg)
{
	int i;
	struct timespec start, end;
	unsigned long long diff;

	clock_gettime(CLOCK_MONOTONIC, &start);

	futex_lock(&futex);
	// rwlock_read_spinlock(&rwlock);
	usleep(1000);
	MBARRIER;
	assert(sum == 0);
	// rwlock_unlock(&rwlock);
	futex_unlock(&futex);

	clock_gettime(CLOCK_MONOTONIC, &end);
	diff = BILLION * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
	__sync_fetch_and_add(&total_time, diff);
}

#define NUM_THREADS 10

int main(void)
{
	pthread_t threads[NUM_THREADS];
	unsigned long i;

	spinlock_init(&spinlock);
	futex_init(&futex);
	rwlock_init(&rwlock);

	for (i = 0; i < NUM_THREADS; i++) {
		pthread_create(&threads[i], NULL, thread_main_writer, (void *)i);
	}

	for (i = 0; i < NUM_THREADS; i++) {
		pthread_join(threads[i], NULL);
	}

	rwlock_read_spinlock(&rwlock);

	for (i = 0; i < NUM_THREADS; i++) {
		pthread_create(&threads[i], NULL, thread_main_reader, (void *)i);
	}

	for (i = 0; i < NUM_THREADS; i++) {
		pthread_join(threads[i], NULL);
	}

	for (i = 0; i < NUM_THREADS; i++) {
		pthread_create(&threads[i], NULL, thread_main_writer, (void *)i);
	}

	rwlock_unlock(&rwlock);

	for (i = 0; i < NUM_THREADS; i++) {
		pthread_join(threads[i], NULL);
	}

#if 1
	for (i = 0; i < NUM_THREADS; i++) {
		if (i % 2 == 0)
			pthread_create(&threads[i], NULL, thread_main_writer, (void *)i);
		else
			pthread_create(&threads[i], NULL, thread_main_reader, (void *)i);
	}

	for (i = 0; i < NUM_THREADS; i++) {
		pthread_join(threads[i], NULL);
	}
#endif

	assert(sum == 0);
	printf("time: %llu\n", total_time);
}
