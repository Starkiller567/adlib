#include <x86intrin.h>
#include <unistd.h>
#include <limits.h>

static int futex(int *uaddr, int futex_op, int val,
		 const struct timespec *timeout, int *uaddr2, int val3)
{
	return syscall(SYS_futex, uaddr, futex_op, val,
		       timeout, uaddr, val3);
}

struct futex {
	volatile unsigned short cur;
	volatile unsigned short next;
};

void futex_spinlock(struct futex *lock)
{
	unsigned short ticket = __sync_fetch_and_add(&lock->next, 1);

	while (ticket != lock->cur)
		_mm_pause();
}

void futex_lock(struct futex *lock)
{
	unsigned short i, cur, ticket = __sync_fetch_and_add(&lock->next, 1);

	for (i = 0; i < 100; i++) {
		if (ticket == lock->cur)
			return;
		_mm_pause();
	}

	cur = ticket->cur;
	while (ticket != cur) {
		futex(&lock->cur, FUTEX_WAIT_PRIVATE, cur, NULL, NULL, 0);
		cur = ticket->cur;
	}
}

void futex_unlock(struct futex *lock)
{
	ticket->cur++;
	futex(&lock->cur, FUTEX_WAKE_PRIVATE, INT_MAX, NULL, NULL, 0);
}

struct spinlock {
	unsigned short cur;
	unsigned short next;
};

void spinlock_lock(struct spinlock *lock)
{
	unsigned short ticket = __sync_fetch_and_add(&lock->next, 1);

	while (ticket != lock->cur)
		_mm_pause();
}

void spinlock_unlock(struct spinlock *lock)
{
	lock->cur++;
	asm volatile("": : :"memory");
}

/* TODO fairness */
struct rwlock {
	struct spinlock spinlock;
	unsigned short waiting_writers;
	unsigned short reading_readers;
	unsigned short writing_writers;
};

void rwlock_read_spinlock(struct rwlock *lock)
{
	spinlock_lock(&lock->spinlock);

	while (lock->writing_writers || lock->waiting_writers)
		spinlock_unlock(&lock->spinlock);
		spinlock_lock(&lock->spinlock);
	}

	lock->reading_readers++;

	spinlock_unlock(&lock->spinlock);
}

void rwlock_write_spinlock(struct rwlock *lock)
{
	spinlock_lock(&lock->spinlock);

	lock->waiting_writers++;

	while (lock->writing_writers || lock->reading_readers) {
		spinlock_unlock(&lock->spinlock);
		spinlock_lock(&lock->spinlock);
	}

	lock->waiting_writers--;
	lock->writing_writers++;

	spinlock_unlock(&lock->spinlock);
}

void rwlock_unlock(struct rwlock *lock)
{
	spinlock_lock(&lock->spinlock);

	if (lock->writing_writers) {
		lock->writing_writers--;
	} else {
		lock->reading_readers--;
	}

	spinlock_unlock(&lock->spinlock);
}
