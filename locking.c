/*
 * TODO:
 * -Once in a while the test program takes significantly longer.
 *  Is it the kernel or a race condition in the rwlock code?
 * -Try using an internal ticketlock for rwlocks
 * -ReadOnce and WriteOnce instead of all the volatiles?
 *
 * NOTE:
 * -Spinlock versions are slower under high contention, don't use them unless
 *  you know what you are doing!
 * -Don't reorder lines in this file!
 * -Some of the MBARRIERs are probably unnecessary, but better safe than sorry
 * -The fences are probably not unnecessary, just highly pessimistic!
 * -For intention locks you have to call the right unlock funtion!
 * -Zeroing a lock == Initializing
 * -Reads and writes are assumed to be atomic, which requires alignment on x64
 */

#include <x86intrin.h>
#include <unistd.h>
#include <limits.h>
#include <linux/futex.h>
#include <sys/time.h>
#include <sys/syscall.h>

#define MBARRIER asm volatile("": : :"memory")

#define READER_FLAG              0x1
#define WRITER_FLAG              0x2
#define I_READER_FLAG            0x4
#define I_WRITER_FLAG            0x8
#define READER_AND_I_WRITER_FLAG 0x16

static inline int sys_futex(int *uaddr, int futex_op, int val,
			    const struct timespec *timeout, int *uaddr2, int val3)
{
	return syscall(SYS_futex, uaddr, futex_op, val,
		       timeout, uaddr, val3);
}

static inline void wait_on_bitset(volatile int *uaddr, int val, int bitset)
{
	sys_futex((int *)uaddr, FUTEX_WAIT_BITSET_PRIVATE, val, NULL, NULL, bitset);
}

static inline void wake_by_bitset(volatile int *uaddr, int val, int bitset)
{
	sys_futex((int *)uaddr, FUTEX_WAKE_BITSET_PRIVATE, val, NULL, NULL, bitset);
}

static void wait_on_ticket(volatile int *uaddr, int val, unsigned int ticket)
{
	int bitset = 1 << (ticket % (sizeof(int) * 8));

	wait_on_bitset(uaddr, val, bitset);
}

static void wake_by_ticket(volatile int *uaddr, unsigned int ticket)
{
	int bitset = 1 << (ticket % (sizeof(int) * 8));

	wake_by_bitset(uaddr, INT_MAX, bitset);
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

int futex_trylock(struct futex *lock)
{
	unsigned int cur = lock->cur;

	return __sync_bool_compare_and_swap(&lock->next_ticket, cur, cur + 1);
}

void futex_unlock(struct futex *lock)
{
	unsigned int wake = lock->cur + 1;

	MBARRIER;
	lock->cur++;
	MBARRIER;
	if (wake != lock->next_ticket)
		wake_by_ticket(&lock->cur, wake);
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

/* TODO fairness? */
struct semaphore {
	volatile unsigned int count;
};

void semaphore_init(struct semaphore *sem, unsigned int count)
{
	sem->count = count;
}

void semaphore_up(struct semaphore *sem)
{
	unsigned int count = __sync_add_and_fetch(&sem->count, 1);

	if (count == 1)
		sys_futex((int *)&sem->count, FUTEX_WAKEUP_PRIVATE,
			  1, NULL, NULL, 0);
}

void semaphore_down(struct semaphore *sem)
{
	unsigned int count;

	do {
		count = sem->count;
		MBARRIER;
		if (count == 0) {
			sys_futex((int *)&sem->count, FUTEX_WAIT_PRIVATE,
				  count, NULL, NULL, 0);
			continue;
		}
	} while (!__sync_bool_compare_and_swap(&sem->count, count, count - 1));
}

struct rwlock {
	volatile unsigned int num_readers; /* -1 if a writer holds the lock */
	volatile unsigned int waiting_readers;
	volatile unsigned int waiting_writers;
};

void rwlock_init(struct rwlock *lock)
{
	lock->num_readers = 0;
	lock->waiting_readers = 0;
	lock->waiting_writers = 0;
}

int rwlock_read_trylock(struct rwlock *lock)
{
	unsigned int num_readers = lock->num_readers;

	MBARRIER;
	if (num_readers == -1 || lock->waiting_writers != 0) {
		return 0;
	}

	return __sync_bool_compare_and_swap(&lock->num_readers, num_readers, num_readers + 1));
}

int rwlock_write_trylock(struct rwlock *lock)
{
	unsigned int num_readers = lock->num_readers;

	MBARRIER;
	if (num_readers != 0) {
		return 0;
	}

	return __sync_bool_compare_and_swap(&lock->num_readers, num_readers, -1));
}

void rwlock_read_spinlock(struct rwlock *lock)
{
	unsigned int num_readers;

	do {
		num_readers = lock->num_readers;
		MBARRIER;
		if (num_readers == -1 || lock->waiting_writers != 0) {
			_mm_pause();
			continue;
		}
	} while(!__sync_bool_compare_and_swap(&lock->num_readers, num_readers, num_readers + 1));
}

void rwlock_write_spinlock(struct rwlock *lock)
{
	unsigned int num_readers;

	if (rwlock_write_trylock(lock)) {
		return;
	}

	__sync_fetch_and_add(&lock->waiting_writers, 1);
	do {
		num_readers = lock->num_readers;
		MBARRIER;
		if (num_readers != 0) {
			_mm_pause();
			continue;
		}
	} while(!__sync_bool_compare_and_swap(&lock->num_readers, num_readers, -1));
	__sync_fetch_and_sub(&lock->waiting_writers, 1);
}

void rwlock_read_lock(struct rwlock *lock)
{
	unsigned int num_readers;

	if (rwlock_read_trylock(lock)) {
		return;
	}

	__sync_fetch_and_add(&lock->waiting_readers, 1);
	do {
		num_readers = lock->num_readers;
		MBARRIER;
		if (lock->waiting_writers != 0 || num_readers == -1) {
			wait_on_bitset(&lock->num_readers, num_readers, READER_FLAG);
			continue;
		}
	} while(!__sync_bool_compare_and_swap(&lock->num_readers, num_readers, num_readers + 1));
	__sync_fetch_and_sub(&lock->waiting_readers, 1);
}

void rwlock_write_lock(struct rwlock *lock)
{
	unsigned int num_readers;

	if (rwlock_write_trylock(lock)) {
		return;
	}

	__sync_fetch_and_add(&lock->waiting_writers, 1);
	do {
		num_readers = lock->num_readers;
		MBARRIER;
		if (num_readers != 0) {
			wait_on_bitset(&lock->num_readers, num_readers, WRITER_FLAG);
			continue;
		}
	} while(!__sync_bool_compare_and_swap(&lock->num_readers, num_readers, -1));
	__sync_fetch_and_sub(&lock->waiting_writers, 1);
}

void rwlock_unlock(struct rwlock *lock)
{
	if (lock->num_readers == -1) {
		lock->num_readers = 0;
	} else {
		__sync_fetch_and_sub(&lock->num_readers, 1);
	}

	if (lock->waiting_writers != 0) {
		if (lock->num_readers == 0) {
			wake_by_bitset(&lock->num_readers, 1, WRITER_FLAG);
		}
	} else if (lock->waiting_readers != 0 && lock->num_readers != -1) {
		wake_by_bitset(&lock->num_readers, INT_MAX, READER_FLAG);
	}
}

struct ilock {
	struct ticketlock lock;
	volatile unsigned int num_readers;
	volatile unsigned int num_i_readers;
	volatile unsigned int num_i_writers;
	volatile unsigned int futex_val;
	volatile unsigned int waiting_readers;
	volatile unsigned int waiting_writers;
	volatile unsigned int waiting_i_readers;
	volatile unsigned int waiting_i_writers;
};

void ilock_init(struct ilock *lock)
{
	ticketlock_init(&lock->lock);
	lock->num_readers = 0;
	lock->num_i_readers = 0;
	lock->num_i_writers = 0;
	lock->futex_val = 0;
	lock->waiting_readers = 0;
	lock->waiting_writers = 0;
	lock->waiting_i_readers = 0;
	lock->waiting_i_writers = 0;
}

static inline int ilock_reader_can_go(struct ilock *lock)
{
	return lock->waiting_i_writers == 0 && lock->waiting_writers == 0 &&
		lock->num_i_writers == 0 && lock->num_readers != -1;
}

static inline int ilock_writer_can_go(struct ilock *lock)
{
	return lock->num_readers == 0 && lock->num_i_readers == 0 &&
		lock->num_i_writers == 0;
}

static inline int ilock_i_reader_can_go(struct ilock *lock)
{
	return lock->waiting_writers == 0 && lock->num_readers != -1;
}

static inline int ilock_i_writer_can_go(struct ilock *lock)
{
	return lock->waiting_writers == 0 && lock->num_readers == 0;
}

static inline int ilock_reader_and_i_writer_can_go(struct ilock *lock)
{
	return lock->waiting_writers == 0 && lock->num_readers == 0 &&
		lock->num_i_writers == 0;
}

void ilock_read_lock(struct ilock *lock)
{
	unsigned int futex_val;

	ticketlock_lock(&lock->lock);

	for (;;) {
		futex_val = lock->futex_val;
		if (ilock_reader_can_go(lock)) {
			break;
		}
		lock->waiting_readers++;
		MBARRIER;
		_mm_mfence();
		ticketlock_unlock(&lock->lock);
		wait_on_bitset(&lock->futex_val, futex_val, READER_FLAG);
		ticketlock_lock(&lock->lock);
		lock->waiting_readers--;
	}

	lock->num_readers++;
	_mm_sfence();
	ticketlock_unlock(&lock->lock);
}

void ilock_write_lock(struct ilock *lock)
{
	unsigned int futex_val;

	ticketlock_lock(&lock->lock);

	for (;;) {
		futex_val = lock->futex_val;
		if (ilock_writer_can_go(lock)) {
			break;
		}
		lock->waiting_writers++;
		MBARRIER;
		_mm_mfence();
		ticketlock_unlock(&lock->lock);
		wait_on_bitset(&lock->futex_val, futex_val, WRITER_FLAG);
		ticketlock_lock(&lock->lock);
		lock->waiting_writers--;
	}

	lock->num_readers = -1;
	_mm_sfence();
	ticketlock_unlock(&lock->lock);
}

void ilock_intention_read_lock(struct ilock *lock)
{
	unsigned int futex_val;

	ticketlock_lock(&lock->lock);

	for (;;) {
		futex_val = lock->futex_val;
		if (ilock_i_reader_can_go(lock)) {
			break;
		}
		lock->waiting_i_readers++;
		MBARRIER;
		_mm_mfence();
		ticketlock_unlock(&lock->lock);
		wait_on_bitset(&lock->futex_val, futex_val, I_READER_FLAG);
		ticketlock_lock(&lock->lock);
		lock->waiting_i_readers--;
	}

	lock->num_i_readers++;
	_mm_sfence();
	ticketlock_unlock(&lock->lock);
}

void ilock_intention_write_lock(struct ilock *lock)
{
	unsigned int futex_val;

	ticketlock_lock(&lock->lock);
	lock->active_writers++;

	for (;;) {
		futex_val = lock->futex_val;
		if (ilock_i_writer_can_go(lock)) {
			break;
		}
		lock->waiting_i_writers++;
		MBARRIER;
		_mm_mfence();
		ticketlock_unlock(&lock->lock);
		wait_on_bitset(&lock->futex_val, futex_val, I_WRITER_FLAG);
		ticketlock_lock(&lock->lock);
		lock->waiting_i_writers--;
	}

	lock->num_i_writers++;
	_mm_sfence();
	ticketlock_unlock(&lock->lock);
}

void ilock_read_and_intention_write_lock(struct ilock *lock)
{
	unsigned int futex_val, read_locked, i_write_locked;

	ticketlock_lock(&lock->lock);

	for (;;) {
		...
	}

	_mm_sfence();
	ticketlock_unlock(&lock->lock);
}

void ilock_read_unlock(struct ilock *lock)
{
	ticketlock_lock(&lock->lock);
	lock->num_readers--;
	lock->futex_val++;
	MBARRIER;
	_mm_sfence();
	ticketlock_unlock(&lock->lock);
	if (lock->waiting_writers != 0) {
		if (lock->num_readers == 0 && lock->num_i_readers == 0) {
			wake_by_bitset(&lock->futex_val, 1, WRITER_FLAG);
		}
	} else if (lock->waiting_i_writers != 0) {
		if (lock->num_readers == 0) {
			wake_by_bitset(&lock->futex_val, INT_MAX, I_WRITER_FLAG);
		}
	}
}

void ilock_write_unlock(struct ilock *lock)
{
	ticketlock_lock(&lock->lock);
	lock->num_readers = 0;
	lock->futex_val++;
	MBARRIER;
	_mm_sfence();
	ticketlock_unlock(&lock->lock);
	sys_futex((int *)&lock->futex_val, FUTEX_WAKE_PRIVATE,
		  INT_MAX, NULL, NULL, 0);
}

void ilock_intention_read_unlock(struct ilock *lock)
{
	ticketlock_lock(&lock->lock);
	lock->num_i_readers--;
	lock->futex_val++;
	MBARRIER;
	_mm_sfence();
	ticketlock_unlock(&lock->lock);
	sys_futex((int *)&lock->futex_val, FUTEX_WAKE_PRIVATE,
		  INT_MAX, NULL, NULL, 0);
}

void ilock_intention_write_unlock(struct ilock *lock)
{
	ticketlock_lock(&lock->lock);
	lock->num_i_writers--;
	lock->futex_val++;
	MBARRIER;
	_mm_sfence();
	ticketlock_unlock(&lock->lock);
	sys_futex((int *)&lock->futex_val, FUTEX_WAKE_PRIVATE,
		  INT_MAX, NULL, NULL, 0);
}

void ilock_read_and_intention_write_unlock(struct ilock *lock)
{
	ticketlock_lock(&lock->lock);
	lock->num_readers--;
	lock->num_i_writers--;
	lock->futex_val++;
	MBARRIER;
	_mm_sfence();
	ticketlock_unlock(&lock->lock);
	sys_futex((int *)&lock->futex_val, FUTEX_WAKE_PRIVATE,
		  INT_MAX, NULL, NULL, 0);
}

#if 0

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <assert.h>

static unsigned long long total_time = 0;
static volatile unsigned long sum = 0;
static struct spinlock spinlock;
static struct futex futex;
static struct rwlock rwlock;
static struct ilock ilock;

#define BILLION 1000000000L

void *thread_main_writer(void *arg)
{
	struct timespec start, end;
	unsigned long long diff;
	usleep(1000);
	clock_gettime(CLOCK_MONOTONIC, &start);

	ilock_write_lock(&ilock);
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
	printf("%p\n", arg);
	ilock_write_unlock(&ilock);

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

	ilock_read_lock(&ilock);
	usleep(10000);
	MBARRIER;
	printf("%p\n", arg);
	// assert(sum == 0);
	ilock_read_unlock(&ilock);

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
	ilock_init(&ilock);

#if 0
	for (i = 0; i < NUM_THREADS; i++) {
		pthread_create(&threads[i], NULL, thread_main_writer, (void *)i);
	}

	for (i = 0; i < NUM_THREADS; i++) {
		pthread_join(threads[i], NULL);
	}

	ilock_intention_write_lock(&ilock);
	ilock_intention_write_lock(&ilock);

	for (i = 0; i < NUM_THREADS; i++) {
		pthread_create(&threads[i], NULL, thread_main_reader, (void *)i);
	}

	ilock_intention_write_unlock(&ilock);
	ilock_intention_write_unlock(&ilock);
	ilock_intention_read_lock(&ilock);
	ilock_intention_read_lock(&ilock);

	for (i = 0; i < NUM_THREADS; i++) {
		pthread_join(threads[i], NULL);
	}

	for (i = 0; i < NUM_THREADS; i++) {
		pthread_create(&threads[i], NULL, thread_main_writer, (void *)i);
	}

	ilock_intention_read_unlock(&ilock);
	ilock_intention_read_unlock(&ilock);

	for (i = 0; i < NUM_THREADS; i++) {
		pthread_join(threads[i], NULL);
	}
#else
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
#endif
