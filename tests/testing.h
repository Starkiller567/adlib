#ifndef __TESTING_INCLUDE__
#define __TESTING_INCLUDE__

#include "compiler.h"
#include <stdbool.h>
#include <stdint.h>

_Static_assert(HAVE_ATTR_CONSTRUCTOR, "");


#define SIMPLE_TEST(name)						\
	static bool test_##name(void);					\
	_attr_constructor void register_simple_test_##name(void)	\
	{								\
		register_simple_test(#name, test_##name);		\
	}								\
	static bool test_##name(void)

#define RANGE_TEST(name, start_, end_)					\
	static bool test_##name(uint64_t start, uint64_t end);		\
	_attr_constructor void register_simple_test_##name(void)	\
	{								\
		register_range_test(#name, start_, end_, test_##name);	\
	}								\
	static bool test_##name(uint64_t start, uint64_t end)

#define RANDOM_TEST(name, num_values, min_value, max_value)		\
	static bool test_##name(uint64_t random);			\
	_attr_constructor void register_simple_test_##name(void)	\
	{								\
		register_random_test(#name, num_values, min_value, max_value, test_##name); \
	}								\
	static bool test_##name(uint64_t random)


void register_simple_test(const char *name, bool (*f)(void));
void register_range_test(const char *name, uint64_t start, uint64_t end,
			 bool (*f)(uint64_t start, uint64_t end));
void register_random_test(const char *name, uint64_t num_values, uint64_t min_value, uint64_t max_value,
			  bool (*f)(uint64_t random));

#endif
