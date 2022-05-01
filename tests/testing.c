#include <assert.h>
#include <inttypes.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <threads.h>
#include "macros.h"
#include "random.h"
#include "testing.h"

static uint64_t global_seed = 0xdeadbeef;

struct simple_test {
	bool (*f)(void);
};

struct range_test {
	uint64_t start;
	uint64_t end;
	bool (*f)(uint64_t start, uint64_t end);
};

struct random_test {
	uint64_t num_values;
	uint64_t min_value;
	uint64_t max_value;
	bool (*f)(uint64_t random);
};

struct test {
	enum {
		TEST_TYPE_SIMPLE,
		TEST_TYPE_RANGE,
		TEST_TYPE_RANDOM,
	} type;
	atomic_bool failed;
	bool visited;
	const char *name;
	union {
		struct simple_test simple_test;
		struct range_test range_test;
		struct random_test random_test;
	};
};

static struct test *tests;
static size_t num_tests;
static size_t tests_capacity;

static void add_test(struct test test)
{
	if (tests_capacity == num_tests) {
		tests_capacity *= 2;
		if (tests_capacity < 8) {
			tests_capacity = 8;
		}
		tests = realloc(tests, tests_capacity * sizeof(tests[0]));
		if (!tests) {
			fputs("realloc failed", stderr);
			abort();
		}
	}
	tests[num_tests++] = test;
}

void register_simple_test(const char *name, bool (*f)(void))
{
	add_test((struct test){
			.type = TEST_TYPE_SIMPLE,
			.name = name,
			.simple_test = (struct simple_test){
				.f = f,
			}
		});
}

void register_range_test(const char *name, uint64_t start, uint64_t end,
			 bool (*f)(uint64_t start, uint64_t end))
{
	add_test((struct test){
			.type = TEST_TYPE_RANGE,
			.name = name,
			.range_test = (struct range_test){
				.start = start,
				.end = end,
				.f = f,
			}
		});
}

void register_random_test(const char *name, uint64_t num_values, uint64_t min_value, uint64_t max_value,
			  bool (*f)(uint64_t random))
{
	add_test((struct test){
			.type = TEST_TYPE_RANDOM,
			.name = name,
			.random_test = (struct random_test){
				.num_values = num_values,
				.min_value = min_value,
				.max_value = max_value,
				.f = f,
			}
		});
}

struct work {
	struct work *next;
	void (*run)(struct work *work);
};

struct test_work {
	struct work work;
	struct test *test;
	bool success;
};

struct range_test_work {
	struct test_work test_work;
	uint64_t start;
	uint64_t end;
};

struct random_test_work {
	struct test_work test_work;
	uint64_t num_values;
	uint64_t failed_value;
};

static void run_simple_test(struct work *_work)
{
	struct test_work *work = container_of(_work, struct test_work, work);
	work->success = work->test->simple_test.f();
	if (!work->success) {
		work->test->failed = true;
	}
}

static void run_range_test(struct work *_work)
{
	struct test_work *work = container_of(_work, struct test_work, work);
	struct range_test_work *range_work = container_of(work, struct range_test_work, test_work);
	work->success = work->test->range_test.f(range_work->start, range_work->end);
	if (!work->success) {
		work->test->failed = true;
	}
}

static void run_random_test(struct work *_work)
{
	struct test_work *work = container_of(_work, struct test_work, work);
	struct random_test_work *random_work = container_of(work, struct random_test_work, test_work);
	uint64_t min_value = work->test->random_test.min_value;
	uint64_t max_value = work->test->random_test.max_value;
	struct random_state rng;
	random_state_init(&rng, random_work->num_values ^ global_seed);
	for (uint64_t i = 0; i < random_work->num_values; i++) {
		uint64_t r = random_next_u64_in_range(&rng, min_value, max_value);
		if (!work->test->random_test.f(r)) {
			work->success = false;
			work->test->failed = true;
			random_work->failed_value = r;
			return;
		}
	}
	work->success = true;
}

struct queue {
	struct work *first;
	struct work **pnext;
};

static void queue_work(struct queue *queue, struct work *work)
{
	*queue->pnext = work;
	queue->pnext = &(*queue->pnext)->next;
}

static void queue_simple_test_work(struct queue *queue, struct test *test, unsigned int nthreads)
{
	struct test_work *test_work = calloc(1, sizeof(*test_work));
	test_work->test = test;
	test_work->work.run = run_simple_test;
	queue_work(queue, &test_work->work);
}

static void queue_range_test_work(struct queue *queue, struct test *test, unsigned int nthreads)
{
	uint64_t start = test->range_test.start;
	uint64_t end = test->range_test.end;
	uint64_t n = end - start;
	if (nthreads - 1 > n) {
		nthreads = n + 1;
	}
	uint64_t per_thread = n / nthreads;
	uint64_t rem = n % nthreads + 1;
	uint64_t cur = start - 1;
	for (unsigned int t = 0; t < nthreads; t++) {
		struct range_test_work *range_test_work = calloc(1, sizeof(*range_test_work));
		range_test_work->test_work.test = test;
		range_test_work->test_work.work.run = run_range_test;
		range_test_work->start = cur + 1;
		cur += per_thread;
		if (rem) {
			cur++;
			rem--;
		}
		range_test_work->end = cur;
		queue_work(queue, &range_test_work->test_work.work);
	}
	assert(cur == end);
}

static void queue_random_test_work(struct queue *queue, struct test *test, unsigned int nthreads)
{
	uint64_t n = test->random_test.num_values;
	if (nthreads > n) {
		nthreads = n;
	}
	uint64_t per_thread = n / nthreads;
	uint64_t rem = n % nthreads;
	uint64_t total = 0;
	for (unsigned int t = 0; t < nthreads; t++) {
		struct random_test_work *random_test_work = calloc(1, sizeof(*random_test_work));
		random_test_work->test_work.test = test;
		random_test_work->test_work.work.run = run_random_test;
		random_test_work->num_values = per_thread;
		if (rem) {
			random_test_work->num_values++;
			rem--;
		}
		total += random_test_work->num_values;
		queue_work(queue, &random_test_work->test_work.work);
	}
	assert(total == n);
}

static void queue_test_work(struct queue *queue, struct test *test, unsigned int nthreads)
{
	switch (test->type) {
	case TEST_TYPE_SIMPLE:
		queue_simple_test_work(queue, test, nthreads);
		break;
	case TEST_TYPE_RANGE:
		queue_range_test_work(queue, test, nthreads);
		break;
	case TEST_TYPE_RANDOM:
		queue_random_test_work(queue, test, nthreads);
		break;
	}
}

static int do_work(void *arg)
{
	struct work **queue = arg;
	for (;;) {
		struct work *work;
		struct work *next;
		do {
			work = atomic_load(queue);
			if (!work) {
				return 0;
			}
			next = work->next;
		} while (!atomic_compare_exchange_strong(queue, &work, next));
		work->run(work);
	}
	return 0;
}

static void run_tests(struct queue queue, unsigned int nthreads)
{
	assert(nthreads > 0);
	thrd_t *threads = alloca((nthreads - 1) * sizeof(threads[0]));
	for (unsigned int t = 0; t < nthreads - 1; t++) {
		if (thrd_create(&threads[t], do_work, &queue) != thrd_success) {
			fputs("thrd_create failed\n", stderr);
			exit(EXIT_FAILURE);
		}
	}
	do_work(&queue);
	for (unsigned int t = 0; t < nthreads - 1; t++) {
		if (thrd_join(threads[t], NULL) != thrd_success) {
			fputs("thrd_join failed\n", stderr);
			exit(EXIT_FAILURE);
		}
	}
}

static void print_results(struct queue queue)
{
	for (struct work *work = queue.first; work; work = work->next) {
		struct test_work *test_work = container_of(work, struct test_work, work);
		struct test *test = test_work->test;
		if (test->visited) {
			continue;
		}
		const char *passed = "\033[32mpassed\033[0m";
		const char *failed = "\033[31mfailed\033[0m";
		if (!test->failed) {
			printf("[%s] %s\n", test->name, passed);
			test->visited = true;
			continue;
		}
		printf("[%s] %s", test->name, test_work->success ? passed : failed);
		switch (test->type) {
		case TEST_TYPE_SIMPLE:
			assert(!test_work->success);
			break;
		case TEST_TYPE_RANGE: {
			struct range_test_work *range_test_work = container_of(test_work,
									       struct range_test_work,
									       test_work);
			printf(" (range: [%" PRIu64 ", %" PRIu64 "])",
			       range_test_work->start, range_test_work->end);
			break;
		}
		case TEST_TYPE_RANDOM: {
			struct random_test_work *random_test_work = container_of(test_work,
										 struct random_test_work,
										 test_work);
			printf("(value: %" PRIu64 ")", random_test_work->failed_value);
			break;
		}
		}
		putchar('\n');
	}
}

int main(int argc, char **argv)
{
	unsigned int nthreads = get_nprocs();

	struct queue queue = {
		.first = NULL,
		.pnext = &queue.first,
	};
	for (size_t i = 0; i < num_tests; i++) {
		if (argc > 1 && !strstr(tests[i].name, argv[1])) {
			continue;
		}
		queue_test_work(&queue, &tests[i], nthreads);
	}
	run_tests(queue, nthreads);
	print_results(queue);
}
