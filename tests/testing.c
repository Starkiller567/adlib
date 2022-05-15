#include <assert.h>
#include <bits/time.h>
#include <inttypes.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/random.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <threads.h>
#include <time.h>
#include <unistd.h>
#include "compiler.h"
#include "macros.h"
#include "random.h"
#include "testing.h"

static uint64_t global_seed = 0;

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
	bool enabled;
	bool should_succeed;
	bool need_subprocess;
	const char *file;
	const char *name;
	union {
		struct simple_test simple_test;
		struct range_test range_test;
		struct random_test random_test;
	};
	struct test_work *work;
	size_t num_work;
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
	const char *file = strrchr(test.file, '/');
	if (file) {
		test.file = file + 1;
	}
	tests[num_tests++] = test;
}

void register_simple_test(const char *file, const char *name,
			  bool (*f)(void), bool should_succeed, bool need_subprocess)
{
	add_test((struct test){
			.type = TEST_TYPE_SIMPLE,
			.file = file,
			.name = name,
			.should_succeed = should_succeed,
			.need_subprocess = need_subprocess,
			.simple_test = (struct simple_test){
				.f = f,
			}
		});
}

void register_range_test(const char *file, const char *name, uint64_t start, uint64_t end,
			 bool (*f)(uint64_t start, uint64_t end), bool should_succeed, bool need_subprocess)
{
	add_test((struct test){
			.type = TEST_TYPE_RANGE,
			.file = file,
			.name = name,
			.should_succeed = should_succeed,
			.need_subprocess = need_subprocess,
			.range_test = (struct range_test){
				.start = start,
				.end = end,
				.f = f,
			}
		});
}

void register_random_test(const char *file, const char *name, uint64_t num_values, uint64_t min_value,
			  uint64_t max_value, bool (*f)(uint64_t random), bool should_succeed,
			  bool need_subprocess)
{
	add_test((struct test){
			.type = TEST_TYPE_RANDOM,
			.file = file,
			.name = name,
			.should_succeed = should_succeed,
			.need_subprocess = need_subprocess,
			.random_test = (struct random_test){
				.num_values = num_values,
				.min_value = min_value,
				.max_value = max_value,
				.f = f,
			}
		});
}

void check_failed(const char *func, const char *file, unsigned int line, const char *cond)
{
	fprintf(stderr, "CHECK failed: %s:%u: %s: %s\n", file, line, func, cond);
}

struct work {
	struct work *next;
	void (*run)(struct work *work);
};

struct test_work {
	struct work work;
	const struct test *test;
	bool passed;
	bool completed;
	uint64_t runtime;
	mtx_t mutex;
	cnd_t cond;
	union {
		struct {
			uint64_t start;
			uint64_t end;
		} range;
		struct {
			uint64_t num_values;
			uint64_t seed;
			uint64_t failed_value;
		} random;
	};
};

static bool run_simple_test(bool (*f)(void))
{
	return f();
}

static bool run_range_test(bool (*f)(uint64_t start, uint64_t end), uint64_t start, uint64_t end)
{
	return f(start, end);
}

static bool run_random_test(bool (*f)(uint64_t random), uint64_t num_values, uint64_t seed,
			    uint64_t min_value, uint64_t max_value, uint64_t *failed_value)
{
	struct random_state rng;
	random_state_init(&rng, seed);

	for (uint64_t i = 0; i < num_values; i++) {
		uint64_t r = random_next_u64_in_range(&rng, min_value, max_value);
		if (!f(r)) {
			*failed_value = r;
			return false;
		}
	}
	return true;
}

static bool run_test_helper(struct test_work *work)
{
	const struct test *test = work->test;
	bool success = false;
	switch (work->test->type) {
	case TEST_TYPE_SIMPLE:
		success = run_simple_test(test->simple_test.f);
		break;
	case TEST_TYPE_RANGE:
		success = run_range_test(test->range_test.f, work->range.start, work->range.end);
		break;
	case TEST_TYPE_RANDOM:
		success = run_random_test(test->random_test.f, work->random.num_values, work->random.seed,
					  test->random_test.min_value, test->random_test.max_value,
					  &work->random.failed_value);
		break;
	}
	return success;
}

static uint64_t ns_elapsed(struct timespec start, struct timespec end)
{
	return (end.tv_sec * 1000000000llu + end.tv_nsec) - (start.tv_sec * 1000000000llu + start.tv_nsec);
}

static void run_test(struct work *_work)
{
	struct test_work *work = container_of(_work, struct test_work, work);
	const struct test *test = work->test;

	struct timespec start, end;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);

	pid_t pid = 0;
	if (test->need_subprocess) {
		pid = fork();
		if (pid == -1) {
			fputs("fork failed\n", stderr);
			exit(EXIT_FAILURE);
		}
	}

	bool success = false;
	if (pid == 0) {
		success = run_test_helper(work);
		if (test->need_subprocess) {
			exit(success ? EXIT_SUCCESS : EXIT_FAILURE);
		}
	} else {
		int status;
		if (waitpid(pid, &status, 0) != pid) {
			fputs("waitpid failed\n", stderr);
			exit(EXIT_FAILURE);
		}
		success = status == EXIT_SUCCESS;
	}
	work->passed = test->should_succeed ? success : !success;

	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
	work->runtime = ns_elapsed(start, end);

	// TODO error checking
	mtx_lock(&work->mutex);
	work->completed = true;
	cnd_signal(&work->cond);
	mtx_unlock(&work->mutex);
}

struct work_queue {
	struct work *first;
	struct work **pnext;
	mtx_t mutex;
	cnd_t cond;
};

static void work_queue_init(struct work_queue *queue)
{
	queue->first = NULL;
	queue->pnext = &queue->first;
	if (mtx_init(&queue->mutex, mtx_plain) != thrd_success) {
		fputs("mtx_init failed\n", stderr);
		exit(EXIT_FAILURE);
	}
	if (cnd_init(&queue->cond) != thrd_success) {
		fputs("cnd_init failed\n", stderr);
		exit(EXIT_FAILURE);
	}
}

static void work_queue_push_work(struct work_queue *queue, struct work *work)
{
	mtx_lock(&queue->mutex); // TODO error checking
	*queue->pnext = work;
	queue->pnext = &work->next;
	cnd_signal(&queue->cond);
	mtx_unlock(&queue->mutex);
}

static struct work *work_queue_pop_work(struct work_queue *queue)
{
	mtx_lock(&queue->mutex); // TODO error checking
	while (!queue->first) {
		cnd_wait(&queue->cond, &queue->mutex);
	}
	struct work *work = queue->first;
	queue->first = work->next;
	if (!queue->first) {
		queue->pnext = &queue->first;
	}
	mtx_unlock(&queue->mutex);
	return work;
}

static struct test_work *test_add_work(struct test *test, size_t n)
{
	struct test_work *test_work = calloc(n, sizeof(*test_work));
	test->work = test_work;
	test->num_work = n;
	for (size_t i = 0; i < n; i++) {
		test_work[i].test = test;
		test_work[i].work.run = run_test;
		if (mtx_init(&test_work[i].mutex, mtx_plain) != thrd_success) {
			fputs("mtx_init failed\n", stderr);
			exit(EXIT_FAILURE);
		}
		if (cnd_init(&test_work[i].cond) != thrd_success) {
			fputs("cnd_init failed\n", stderr);
			exit(EXIT_FAILURE);
		}
	}
	return test_work;
}

static void queue_simple_test_work(struct work_queue *queue, struct test *test, unsigned int nthreads)
{
	struct test_work *test_work = test_add_work(test, 1);
	work_queue_push_work(queue, &test_work->work);
}

static void queue_range_test_work(struct work_queue *queue, struct test *test, unsigned int nthreads)
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
	struct test_work *test_work = test_add_work(test, nthreads);
	for (unsigned int t = 0; t < nthreads; t++) {
		test_work[t].range.start = cur + 1;
		cur += per_thread;
		if (rem) {
			cur++;
			rem--;
		}
		test_work[t].range.end = cur;
		work_queue_push_work(queue, &test_work[t].work);
	}
	assert(cur == end);
}

static uint64_t splitmix64(uint64_t *x)
{
	uint64_t z = (*x += 0x9e3779b97f4a7c15);
	z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
	z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
	return z ^ (z >> 31);
}

static void queue_random_test_work(struct work_queue *queue, struct test *test, unsigned int nthreads)
{
	uint64_t n = test->random_test.num_values;
	if (nthreads > n) {
		nthreads = n;
	}
	uint64_t per_thread = n / nthreads;
	uint64_t rem = n % nthreads;
	uint64_t total = 0;
	struct test_work *test_work = test_add_work(test, nthreads);
	for (unsigned int t = 0; t < nthreads; t++) {
		test_work[t].random.num_values = per_thread;
		if (rem) {
			test_work[t].random.num_values++;
			rem--;
		}
		total += test_work[t].random.num_values;
		test_work[t].random.seed = splitmix64(&global_seed);
		work_queue_push_work(queue, &test_work[t].work);
	}
	assert(total == n);
}

static void queue_test_work(struct work_queue *queue, struct test *test, unsigned int nthreads)
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

static struct work poison_work;

static int do_work(void *arg)
{
	struct work_queue *queue = arg;
	for (;;) {
		struct work *work = work_queue_pop_work(queue);
		if (work == &poison_work) {
			work_queue_push_work(queue, work);
			break;
		}
		work->run(work);
	}
	return 0;
}

struct thread_pool {
	thrd_t *threads;
	size_t nthreads;
	struct work_queue *queue;
};

static void thread_pool_start(struct thread_pool *pool, struct work_queue *queue, size_t nthreads)
{
	assert(nthreads > 0);
	thrd_t *threads = malloc(nthreads * sizeof(threads[0]));
	for (unsigned int t = 0; t < nthreads; t++) {
		if (thrd_create(&threads[t], do_work, queue) != thrd_success) {
			fputs("thrd_create failed\n", stderr);
			exit(EXIT_FAILURE);
		}
	}
	pool->threads = threads;
	pool->nthreads = nthreads;
	pool->queue = queue;
}

static void thread_pool_stop(struct thread_pool *pool)
{
	work_queue_push_work(pool->queue, &poison_work);
	for (unsigned int t = 0; t < pool->nthreads; t++) {
		if (thrd_join(pool->threads[t], NULL) != thrd_success) {
			fputs("thrd_join failed\n", stderr);
			exit(EXIT_FAILURE);
		}
	}
	free(pool->threads);
	memset(pool, 0, sizeof(*pool));
}

static void print_time(uint64_t ns, FILE *file)
{
	double t = ns;
	const char *unit = "ns";
	if (t > 1000000000) {
		t *= 1.0 / 1000000000;
		unit = "s";
	} else if (t > 1000000) {
		t *= 1.0 / 1000000;
		unit = "ms";
	} else if (t > 1000) {
		t *= 1.0 / 1000;
		unit = "us";
	}
	fprintf(file, "%.1f %s", t, unit);
}

static void print_results(void)
{
#define GREEN "\033[32m"
#define RED "\033[31m"
#define CLEAR_LINE "\033[2K\r"
#define RESET "\033[0m"
	const char *passed = GREEN "passed" RESET;
	const char *failed = RED "failed" RESET;

	struct timespec start, end;
	clock_gettime(CLOCK_REALTIME, &start);

	size_t num_failed = 0;
	for (size_t i = 0; i < num_tests; i++) {
		struct test *test = &tests[i];
		assert(test->enabled); // the first num_tests tests should be enabled
		assert(test->num_work != 0); // any enabled test should have work scheduled
		fprintf(stderr, "%zu/%zu", i, num_tests);
		uint64_t runtime = 0;
		bool test_failed = false;
		for (size_t j = 0; j < test->num_work; j++) {
			struct test_work *work = &test->work[j];
			// TODO error checking
			mtx_lock(&work->mutex);
			while (!work->completed) {
				cnd_wait(&work->cond, &work->mutex);
			}
			mtx_unlock(&work->mutex);
			runtime += work->runtime;
			if (!work->passed) {
				test_failed = true;
			}
		}
		if (test_failed) {
			num_failed++;
		}
		fputs(CLEAR_LINE, stderr);
		printf("[%s/%s] %s (", test->file, test->name, test_failed ? failed : passed);
		print_time(runtime, stdout);
		puts(")");
		if (!test_failed) {
			continue;
		}
		for (size_t j = 0; j < test->num_work; j++) {
			struct test_work *work = &test->work[j];
			if (work->passed) {
				continue;
			}
			switch (test->type) {
			case TEST_TYPE_SIMPLE:
				break;
			case TEST_TYPE_RANGE: {
				printf("    failed on range: [%" PRIu64 ", %" PRIu64 "]\n",
				       work->range.start, work->range.end);
				break;
			}
			case TEST_TYPE_RANDOM: {
				printf("    failed on value: %" PRIu64 "\n", work->random.failed_value);
				break;
			}
			}
		}
	}
	clock_gettime(CLOCK_REALTIME, &end);

	printf("Summary: %s%zu/%zu failed" RESET " (",
	       num_failed == 0 ? GREEN : RED, num_failed, num_tests);
	print_time(ns_elapsed(start, end), stdout);
	puts(")");
}

NEGATIVE_SIMPLE_TEST_SUBPROCESS(selftest1)
{
	raise(SIGABRT);
	return true;
}

NEGATIVE_SIMPLE_TEST(selftest2)
{
	return false;
}

RANDOM_TEST(selftest3, 1000, 13, 42)
{
	CHECK(13 <= random && random <= 42);
	return true;
}

RANGE_TEST_SUBPROCESS(selftest4, 13, 17)
{
	CHECK(start <= end);
	CHECK(13 <= start && end <= 17);
	return true;
}

static int compare_tests(const void *_a, const void *_b)
{
	const struct test *a = _a;
	const struct test *b = _b;
	if (a->enabled && !b->enabled) {
		return -1;
	}
	if (!a->enabled && b->enabled) {
		return 1;
	}
	if (!a->enabled && !b->enabled) {
		return 0; // whatever
	}
	int r = strcmp(a->file, b->file);
	if (r != 0) {
		return r;
	}
	return strcmp(a->name, b->name); // TODO maybe sort by order in file instead (using __COUNTER__)?
}

int main(int argc, char **argv)
{
	unsigned int nthreads = 0;
	const char *accept_file_substr = NULL;
	const char *accept_file_exact = NULL;
	const char *accept_name_substr = NULL;
	const char *accept_name_exact = NULL;
	const char *reject_file_substr = NULL;
	const char *reject_file_exact = NULL;
	const char *reject_name_substr = NULL;
	const char *reject_name_exact = NULL;
	bool seed_initialized = false;
	bool reject = false;
	for (;;) {
		int rv = getopt(argc, argv, "j:f:F:t:T:s:n");
		if (rv == -1) {
			break;
		}
		switch (rv) {
		case '?':
			fprintf(stderr, "Usage: %s TODO\n", argv[0]);
			return 1;
		case 'j':
			nthreads = atoi(optarg);
			break;
		case 'n':
			reject = true;
			break;
		case 'f':
			if (reject) {
				reject_file_substr = optarg;
			} else {
				accept_file_substr = optarg;
			}
			break;
		case 'F':
			if (reject) {
				reject_file_exact = optarg;
			} else {
				accept_file_exact = optarg;
			}
			break;
		case 't':
			if (reject) {
				reject_name_substr = optarg;
			} else {
				accept_name_substr = optarg;
			}
			break;
		case 'T':
			if (reject) {
				reject_name_exact = optarg;
			} else {
				accept_name_exact = optarg;
			}
			break;
		case 's':
			global_seed = atoi(optarg);
			seed_initialized = true;
			break;
		}
		if (rv != 'n') {
			reject = false;
		}
	}
	argv += optind;
	argc -= optind;

	if (nthreads == 0) {
		nthreads = get_nprocs();
	}

	if (!seed_initialized) {
		if (getrandom(&global_seed, sizeof(global_seed), 0) == -1) {
			perror("getrandom failed");
			abort();
		}
	}

	struct work_queue queue;
	work_queue_init(&queue);

	size_t num_enabled = 0;
	for (size_t i = 0; i < num_tests; i++) {
		tests[i].enabled = false;
		if (accept_file_substr && !strstr(tests[i].file, accept_file_substr)) {
			continue;
		}
		if (accept_file_exact && strcmp(tests[i].file, accept_file_exact) != 0) {
			continue;
		}
		if (accept_name_substr && !strstr(tests[i].name, accept_name_substr)) {
			continue;
		}
		if (accept_name_exact && strcmp(tests[i].name, accept_name_exact) != 0) {
			continue;
		}
		if (reject_file_substr && strstr(tests[i].file, reject_file_substr)) {
			continue;
		}
		if (reject_file_exact && strcmp(tests[i].file, reject_file_exact) == 0) {
			continue;
		}
		if (reject_name_substr && strstr(tests[i].name, reject_name_substr)) {
			continue;
		}
		if (reject_name_exact && strcmp(tests[i].name, reject_name_exact) == 0) {
			continue;
		}
		tests[i].enabled = true;
		num_enabled++;
	}
	qsort(tests, num_tests, sizeof(tests[0]), compare_tests);
	num_tests = num_enabled;
	for (size_t i = 0; i < num_tests; i++) {
		queue_test_work(&queue, &tests[i], nthreads);
	}

	struct thread_pool thread_pool;
	thread_pool_start(&thread_pool, &queue, nthreads);
	print_results();
	thread_pool_stop(&thread_pool);
	for (size_t i = 0; i < num_tests; i++) {
		free(tests[i].work);
	}
	free(tests);
}
