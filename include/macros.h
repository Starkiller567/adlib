/*
 * Copyright (C) 2020-2021 Fabian Hügel
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef __MACROS_INCLUDE__
#define __MACROS_INCLUDE__

#include <stddef.h>
#include "config.h"

#ifdef HAVE_TYPEOF
# define typeof(x)                           __typeof__(x)
#endif

#ifdef HAVE_ATTR_NONNULL
# define _attr_nonnull(...)                  __attribute__((nonnull (__VA_ARGS__)))
#else
# define _attr_nonnull(...)
#endif

#ifdef HAVE_ATTR_WARN_UNUSED_RESULT
# define _attr_warn_unused_result            __attribute__((warn_unused_result))
#else
# define _attr_warn_unused_result
#endif

#ifdef HAVE_ATTR_FORMAT_PRINTF
# define _attr_format_printf(f, a)           __attribute__((format (printf, (f), (a))))
#else
# define _attr_format_printf(f, a)
#endif

#ifdef HAVE_ATTR_UNUSED
# define _attr_unused                        __attribute__((unused))
#else
# define _attr_unused
#endif

#ifdef HAVE_BUILTIN_EXPECT
# define likely(expr)                        __builtin_expect(!!(expr), 1)
# define unlikely(expr)                      __builtin_expect(!!(expr), 0)
#else
# define likely(expr)                        (expr)
# define unlikely(expr)                      (expr)
#endif

// from wikipedia, the ternary operator forces matching types on pointer and member
#define container_of(ptr, type, member)					\
	((type *)((char *)(1 ? (ptr) : &((type *)0)->member) - offsetof(type, member)))

#endif
