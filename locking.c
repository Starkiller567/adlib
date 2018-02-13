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
	struct spinlock spinlock;
	volatile unsigned int cur;
	volatile unsigned int next_ticket;
	volatile unsigned int num_readers;
	volatile unsigned int n;
};

void rwlock_init(struct rwlock *lock)
{
	spinlock_init(&lock->spinlock);
	lock->cur = 0;
	lock->next_ticket = 0;
	lock->num_readers = 0;
	lock->n = 0;
}

void rwlock_read_spinlock(struct rwlock *lock)
{
	unsigned int ticket;

	spinlock_lock(&lock->spinlock);

	ticket = lock->next_ticket++;
	MBARRIER;
	while (ticket != lock->cur) {
		spinlock_unlock(&lock->spinlock);
		spinlock_lock(&lock->spinlock);
	}

	lock->num_readers++;
	lock->cur++;

	spinlock_unlock(&lock->spinlock);
}

void rwlock_write_spinlock(struct rwlock *lock)
{
	unsigned int ticket;

	spinlock_lock(&lock->spinlock);

	ticket = lock->next_ticket++;
	MBARRIER;
	while (ticket != lock->cur || lock->num_readers != 0) {
		spinlock_unlock(&lock->spinlock);
		spinlock_lock(&lock->spinlock);
	}

	spinlock_unlock(&lock->spinlock);
}

void rwlock_read_lock(struct rwlock *lock)
{
	unsigned int ticket, n;

	spinlock_lock(&lock->spinlock);

	ticket = lock->next_ticket++;
	n = lock->n;
	MBARRIER;
	while (ticket != lock->cur) {
		spinlock_unlock(&lock->spinlock);
		sys_futex((int *)&lock->n, FUTEX_WAIT_PRIVATE, n, NULL, NULL, 0);
		spinlock_lock(&lock->spinlock);
		n = lock->n;
		MBARRIER;
	}

	lock->num_readers++;
	lock->cur++;

	spinlock_unlock(&lock->spinlock);
}

void rwlock_write_lock(struct rwlock *lock)
{
	unsigned int ticket, n;

	spinlock_lock(&lock->spinlock);

	ticket = lock->next_ticket++;
	n = lock->n;
	MBARRIER;
	while (ticket != lock->cur || lock->num_readers != 0) {
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
	spinlock_lock(&lock->spinlock);

	if (lock->num_readers != 0) {
		lock->num_readers--;
	} else {
		lock->cur++;
	}
	MBARRIER;

	lock->n++;
	spinlock_unlock(&lock->spinlock);

	sys_futex((int *)&lock->n, FUTEX_WAKE_PRIVATE, INT_MAX, NULL, NULL, 0);
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

void *thread_main_writer(void *arg)
{
	struct timespec start, end;
	unsigned long long ns;
	usleep(1000);
	clock_gettime(CLOCK_MONOTONIC, &start);

	rwlock_write_lock(&rwlock);
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
	__sync_fetch_and_add(&total_time, end.tv_nsec - start.tv_nsec);
	ns = end.tv_nsec - start.tv_nsec;
	ns += 1000000000 * (end.tv_sec - start.tv_sec);
	__sync_fetch_and_add(&total_time, ns);
}

void *thread_main_reader(void *arg)
{
	int i;
	struct timespec start, end;
	unsigned long long ns;
	usleep(1000);
	clock_gettime(CLOCK_MONOTONIC, &start);

	rwlock_read_lock(&rwlock);
	usleep(300000);
	MBARRIER;
	assert(sum == 0);
	rwlock_unlock(&rwlock);

	clock_gettime(CLOCK_MONOTONIC, &end);
	ns = end.tv_nsec - start.tv_nsec;
	ns += 1000000000 * (end.tv_sec - start.tv_sec);
	__sync_fetch_and_add(&total_time, ns);
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

	printf("sum: %lu\n", sum);
	printf("time: %llu\n", total_time);
}
