/*
 * TODO:
 * -Once in a while the test program takes significantly longer.
 *  Is it the kernel or a race condition in the rwlock code?
 * -Also make throughput versions of these locks in addition to the fair versions.
 */

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

struct ticketlock {
	volatile unsigned int cur;
	volatile unsigned int next_ticket;
};

void ticketlock_init(struct ticketlock *lock)
{
	lock->cur = 0;
	lock->next_ticket = 0;
}

void ticketlock_lock(struct ticketlock *lock)
{
	unsigned int ticket = __sync_fetch_and_add(&lock->next_ticket, 1);

	while (ticket != lock->cur)
		_mm_pause();
}

void ticketlock_unlock(struct ticketlock *lock)
{
	lock->cur++;
	MBARRIER;
}

struct spinlock {
	volatile int locked;
};

void spinlock_init(struct spinlock *lock)
{
	lock->locked = 0;
}

void spinlock_lock(struct spinlock *lock)
{
	while(__sync_lock_test_and_set(&lock->locked, 1))
		_mm_pause();
}

void spinlock_unlock(struct spinlock *lock)
{
	__sync_lock_release(&lock->locked);
}

struct rwlock {
	struct futex futex;
	volatile unsigned int num_readers; /* -1 if a writer holds the lock */
};

void rwlock_init(struct rwlock *lock)
{
	futex_init(&lock->futex);
	lock->num_readers = 0;
}

void rwlock_read_spinlock(struct rwlock *lock)
{
	futex_spinlock(&lock->futex);
	while (lock->num_readers == -1)
		_mm_pause();
	lock->num_readers++;
	futex_unlock(&lock->futex);
}

void rwlock_write_spinlock(struct rwlock *lock)
{
	futex_spinlock(&lock->futex);
	while (lock->num_readers != 0)
		_mm_pause();
	lock->num_readers--;
	futex_unlock(&lock->futex);
}

void rwlock_read_lock(struct rwlock *lock)
{
	unsigned int num_readers;

	futex_lock(&lock->futex);
	num_readers = lock->num_readers;
	MBARRIER;
	while (num_readers == -1) {
		sys_futex((int *)&lock->num_readers, FUTEX_WAIT_PRIVATE, num_readers, NULL, NULL, 0);
		MBARRIER;
		num_readers = lock->num_readers;
		MBARRIER;
	}
	lock->num_readers++;
	futex_unlock(&lock->futex);
}

void rwlock_write_lock(struct rwlock *lock)
{
	unsigned int num_readers;

	futex_lock(&lock->futex);
	num_readers = lock->num_readers;
	MBARRIER;
	while (num_readers != 0) {
		sys_futex((int *)&lock->num_readers, FUTEX_WAIT_PRIVATE, num_readers, NULL, NULL, 0);
		MBARRIER;
		num_readers = lock->num_readers;
		MBARRIER;
	}
	lock->num_readers--;
	futex_unlock(&lock->futex);
}

void rwlock_unlock(struct rwlock *lock)
{
	if (lock->num_readers == -1) {
		lock->num_readers = 0;
	} else {
		__sync_fetch_and_sub(&lock->num_readers, 1);
	}
	MBARRIER;

	/* this technically only needs to happen if someone is waiting */
	sys_futex((int *)&lock->num_readers, FUTEX_WAKE_PRIVATE, 1, NULL, NULL, 0);
}

/* intention lock */
struct ilock {
	struct futex futex;
	volatile unsigned int num_readers; /* -1 if a writer holds the lock */
	volatile unsigned int num_i_readers;
	volatile unsigned int num_i_writers;
	volatile unsigned int num_unlocks;
};

void ilock_init(struct ilock *lock)
{
	futex_init(&lock->futex);
	lock->num_readers = 0;
	lock->num_i_readers = 0;
	lock->num_i_writers = 0;
	lock->num_unlocks = 0;
}

void ilock_read_spinlock(struct ilock *lock)
{
	futex_spinlock(&lock->futex);
	while (lock->num_readers == -1 || lock->num_i_writers != 0)
		_mm_pause();
	lock->num_readers++;
	futex_unlock(&lock->futex);
}

void ilock_write_spinlock(struct ilock *lock)
{
	futex_spinlock(&lock->futex);
	while (lock->num_readers != 0 || lock->num_i_readers != 0 ||
	       lock->num_i_writers != 0)
		_mm_pause();
	lock->num_readers--;
	futex_unlock(&lock->futex);
}

void ilock_intention_read_spinlock(struct ilock *lock)
{
	futex_spinlock(&lock->futex);
	while (lock->num_readers == -1)
		_mm_pause();
	lock->num_i_readers++;
	futex_unlock(&lock->futex);
}

void ilock_intention_write_spinlock(struct ilock *lock)
{
	futex_spinlock(&lock->futex);
	while (lock->num_readers != 0)
		_mm_pause();
	lock->num_i_writers++;
	futex_unlock(&lock->futex);
}

void ilock_read_intention_write_spinlock(struct ilock *lock)
{
	futex_spinlock(&lock->futex);
	while (lock->num_readers != 0 || lock->num_i_writers != 0)
		_mm_pause();
	lock->num_readers++;
	lock->num_i_writers++;
	futex_unlock(&lock->futex);
}

void ilock_read_lock(struct ilock *lock)
{
	unsigned int num_unlocks;

	futex_lock(&lock->futex);
	num_unlocks = lock->num_unlocks;
	MBARRIER;
	while (lock->num_readers == -1 || lock->num_i_writers != 0) {
		sys_futex((int *)&lock->num_unlocks, FUTEX_WAIT_PRIVATE, num_unlocks, NULL, NULL, 0);
		MBARRIER;
		num_unlocks = lock->num_unlocks;
		MBARRIER;
	}
	lock->num_readers++;
	futex_unlock(&lock->futex);
}

void ilock_write_lock(struct ilock *lock)
{
	unsigned int num_unlocks;

	futex_lock(&lock->futex);
	num_unlocks = lock->num_unlocks;
	MBARRIER;
	while (lock->num_readers != 0 || lock->num_i_readers != 0 ||
	       lock->num_i_writers != 0) {
		sys_futex((int *)&lock->num_unlocks, FUTEX_WAIT_PRIVATE, num_unlocks, NULL, NULL, 0);
		MBARRIER;
		num_unlocks = lock->num_unlocks;
		MBARRIER;
	}
	lock->num_readers--;
	futex_unlock(&lock->futex);
}

void ilock_intention_read_lock(struct ilock *lock)
{
	unsigned int num_unlocks;

	futex_lock(&lock->futex);
	num_unlocks = lock->num_unlocks;
	MBARRIER;
	while (lock->num_readers == -1) {
		sys_futex((int *)&lock->num_unlocks, FUTEX_WAIT_PRIVATE, num_unlocks, NULL, NULL, 0);
		MBARRIER;
		num_unlocks = lock->num_unlocks;
		MBARRIER;
	}
	lock->num_i_readers++;
	futex_unlock(&lock->futex);
}

void ilock_intention_write_lock(struct ilock *lock)
{
	unsigned int num_unlocks;

	futex_lock(&lock->futex);
	num_unlocks = lock->num_unlocks;
	MBARRIER;
	while (lock->num_readers != 0) {
		sys_futex((int *)&lock->num_unlocks, FUTEX_WAIT_PRIVATE, num_unlocks, NULL, NULL, 0);
		MBARRIER;
		num_unlocks = lock->num_unlocks;
		MBARRIER;
	}
	lock->num_i_writers++;
	futex_unlock(&lock->futex);
}

void ilock_read_and_intention_write_lock(struct ilock *lock)
{
	unsigned int num_unlocks;

	futex_lock(&lock->futex);
	num_unlocks = lock->num_unlocks;
	MBARRIER;
	while (lock->num_readers != 0 || lock->num_i_writers != 0) {
		sys_futex((int *)&lock->num_unlocks, FUTEX_WAIT_PRIVATE, num_unlocks, NULL, NULL, 0);
		MBARRIER;
		num_unlocks = lock->num_unlocks;
		MBARRIER;
	}
	lock->num_readers++;
	lock->num_i_writers++;
	futex_unlock(&lock->futex);
}

void ilock_read_unlock(struct ilock *lock)
{
	/* TODO this needs to happen atomically!!! */
	lock->num_readers--;
	_mm_sfence();
	MBARRIER;
	lock->num_unlocks++;
	sys_futex((int *)&lock->num_unlocks, FUTEX_WAKE_PRIVATE, 1, NULL, NULL, 0);
}

void ilock_write_unlock(struct ilock *lock)
{
	/* only one thread can hold this kind of lock at a time */
	lock->num_readers = 0;
	_mm_sfence();
	MBARRIER;
	/* TODO this increment needs to happen atomically */
	lock->num_unlocks++;
	sys_futex((int *)&lock->num_unlocks, FUTEX_WAKE_PRIVATE, 1, NULL, NULL, 0);
}

void ilock_intention_read_unlock(struct ilock *lock)
{
	/* TODO this needs to happen atomically!!! */
	lock->num_i_readers--;
	_mm_sfence();
	MBARRIER;
	lock->num_unlocks++;
	sys_futex((int *)&lock->num_unlocks, FUTEX_WAKE_PRIVATE, 1, NULL, NULL, 0);
}

void ilock_intention_write_unlock(struct ilock *lock)
{
	/* TODO this needs to happen atomically!!! */
	lock->num_i_writers--;
	_mm_sfence();
	MBARRIER;
	lock->num_unlocks++;
	sys_futex((int *)&lock->num_unlocks, FUTEX_WAKE_PRIVATE, 1, NULL, NULL, 0);
}

void ilock_read_and_intention_write_unlock(struct ilock *lock)
{
	/* only one thread can hold this kind of lock at a time */
	lock->num_readers--;
	lock->num_i_writers--;
	_mm_sfence();
	MBARRIER;
	/* TODO this increment needs to happen atomically */
	lock->num_unlocks++;
	sys_futex((int *)&lock->num_unlocks, FUTEX_WAKE_PRIVATE, 1, NULL, NULL, 0);
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

	rwlock_write_spinlock(&rwlock);
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
	rwlock_unlock(&rwlock);

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

	rwlock_read_spinlock(&rwlock);
	usleep(10000);
	MBARRIER;
	assert(sum == 0);
	rwlock_unlock(&rwlock);

	clock_gettime(CLOCK_MONOTONIC, &end);
	diff = BILLION * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
	__sync_fetch_and_add(&total_time, diff);
}

#define NUM_THREADS 100

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
