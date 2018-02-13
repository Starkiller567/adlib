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

struct futex {
	volatile unsigned int cur;
	volatile unsigned int next;
};

void futex_init(struct futex *lock)
{
	lock->cur = 0;
	lock->next = 0;
}

void futex_spinlock(struct futex *lock)
{
	unsigned int ticket = __sync_fetch_and_add(&lock->next, 1);

	while (ticket != lock->cur)
		_mm_pause();
}

void futex_lock(struct futex *lock)
{
	unsigned int i, cur, ticket = __sync_fetch_and_add(&lock->next, 1);

	for (i = 0; i < 100; i++) {
		if (ticket == lock->cur)
			return;
		_mm_pause();
	}

	cur = lock->cur;
	MBARRIER;
	while (ticket != cur) {
		sys_futex((int *)&lock->cur, FUTEX_WAIT_PRIVATE, cur, NULL, NULL, 0);
		cur = lock->cur;
		MBARRIER;
	}
}

void futex_unlock(struct futex *lock)
{
	lock->cur++;
	MBARRIER;
	sys_futex((int *)&lock->cur, FUTEX_WAKE_PRIVATE, INT_MAX, NULL, NULL, 0);
}

struct spinlock {
	volatile unsigned int cur;
	volatile unsigned int next;
};

void spinlock_init(struct spinlock *lock)
{
	lock->cur = 0;
	lock->next = 0;
}

void spinlock_lock(struct spinlock *lock)
{
	unsigned int ticket = __sync_fetch_and_add(&lock->next, 1);

	while (ticket != lock->cur)
		_mm_pause();
}

void spinlock_unlock(struct spinlock *lock)
{
	lock->cur++;
	MBARRIER;
}

struct rwlock {
	struct spinlock spinlock;
	volatile unsigned int cur;
	volatile unsigned int next;
	volatile unsigned int reading_readers;
	volatile unsigned int n;
};

void rwlock_init(struct rwlock *lock)
{
	spinlock_init(&lock->spinlock);
	lock->cur = 0;
	lock->next = 0;
	lock->reading_readers = 0;
	lock->n = 0;
}

void rwlock_read_spinlock(struct rwlock *lock)
{
	unsigned int ticket;

	printf("read lock\n");
	spinlock_lock(&lock->spinlock);

	ticket = lock->next++;
	MBARRIER;
	while (ticket != lock->cur) {
		spinlock_unlock(&lock->spinlock);
		spinlock_lock(&lock->spinlock);
	}

	lock->reading_readers++;
	lock->cur++;

	spinlock_unlock(&lock->spinlock);
}

void rwlock_write_spinlock(struct rwlock *lock)
{
	unsigned int ticket;

	printf("write lock\n");
	spinlock_lock(&lock->spinlock);

	ticket = lock->next++;
	MBARRIER;
	while (ticket != lock->cur || lock->reading_readers) {
		spinlock_unlock(&lock->spinlock);
		spinlock_lock(&lock->spinlock);
	}

	spinlock_unlock(&lock->spinlock);
}

void rwlock_read_lock(struct rwlock *lock)
{
	unsigned int ticket, n;

	spinlock_lock(&lock->spinlock);

	ticket = lock->next++;
	n = lock->n;
	MBARRIER;
	while (ticket != lock->cur) {
		spinlock_unlock(&lock->spinlock);
		sys_futex((int *)&lock->n, FUTEX_WAIT_PRIVATE, n, NULL, NULL, 0);
		spinlock_lock(&lock->spinlock);
		n = lock->n;
		MBARRIER;
	}

	lock->reading_readers++;
	lock->cur++;

	spinlock_unlock(&lock->spinlock);
}

void rwlock_write_lock(struct rwlock *lock)
{
	unsigned int ticket, n;

	spinlock_lock(&lock->spinlock);

	ticket = lock->next++;
	n = lock->n;
	MBARRIER;
	while (ticket != lock->cur || lock->reading_readers) {
		spinlock_unlock(&lock->spinlock);
		sys_futex((int *)&lock->n, FUTEX_WAIT_PRIVATE, n, NULL, NULL, 0);
		spinlock_lock(&lock->spinlock);
		n = lock->n;
		MBARRIER;
	}

	spinlock_unlock(&lock->spinlock);
}

void rwlock_unlock(struct rwlock *lock)
{
	printf("unlock\n");
	spinlock_lock(&lock->spinlock);

	if (lock->reading_readers) {
		lock->reading_readers--;
	} else {
		lock->cur++;
	}
	MBARRIER;

	lock->n++;
	MBARRIER;
	spinlock_unlock(&lock->spinlock);

	sys_futex((int *)&lock->n, FUTEX_WAKE_PRIVATE, INT_MAX, NULL, NULL, 0);
}

#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <assert.h>

static time_t total_time = 0;
static unsigned long sum = 0;
static struct spinlock spinlock;
static struct futex futex;
static struct rwlock rwlock;

void *thread_main_writer(void *arg)
{
	time_t time;
	sleep(1);
	time = clock();

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

	time = clock() - time;
	__sync_fetch_and_add(&total_time, time);
}

void *thread_main_reader(void *arg)
{
	sleep(1);
	rwlock_read_spinlock(&rwlock);
	assert(sum == 0);
	rwlock_unlock(&rwlock);
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

	sleep(3);
	rwlock_read_spinlock(&rwlock);
	printf("sum: %lu\n", sum);

	for (i = 0; i < NUM_THREADS; i++) {
		pthread_join(threads[i], NULL);
	}

	for (i = 0; i < NUM_THREADS / 2; i++) {
		pthread_create(&threads[i], NULL, thread_main_reader, (void *)i);
	}
	for (i = NUM_THREADS / 2; i < NUM_THREADS; i++) {
		pthread_create(&threads[i], NULL, thread_main_writer, (void *)i);
	}

	printf("sum: %lu\n", sum);
	for (i = 0; i < NUM_THREADS / 2; i++) {
		pthread_join(threads[i], NULL);
	}
	printf("sum: %lu\n", sum);
	printf("time: %lu\n", total_time);

	// pthread_join(threads[NUM_THREADS / 2], NULL);

}
