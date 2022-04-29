/*
 * Copyright (C) 2020-2022 Fabian Hügel
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


#ifndef __CONFIG_COMPILER__
#define __CONFIG_COMPILER__

#include "config.h"

#ifndef __has_attribute
  #define __has_attribute(x) 0
#endif

#ifndef __has_builtin
  #define __has_builtin(x) 0
#endif

#if __has_attribute(nonnull)
# define HAVE_ATTR_NONNULL 1
#endif
#if __has_attribute(warn_unused_result)
# define HAVE_ATTR_NODISCARD 1
#endif
#if __has_attribute(always_inline)
# define HAVE_ATTR_ALWAYS_INLINE 1
#endif
#if __has_attribute(fallthrough)
# define HAVE_ATTR_FALLTHROUGH 1
#endif
#if __has_attribute(format)
# define HAVE_ATTR_FORMAT_PRINTF 1
#endif
#if __has_attribute(unused)
# define HAVE_ATTR_UNUSED 1
#endif
#if __has_attribute(packed)
# define HAVE_ATTR_PACKED 1
#endif
#if __has_attribute(malloc)
# define HAVE_ATTR_MALLOC 1
#endif
#if __has_attribute(pure)
# define HAVE_ATTR_PURE 1
#endif
#if __has_attribute(const)
# define HAVE_ATTR_CONST 1
#endif
#if __has_attribute(alloc_size)
# define HAVE_ATTR_ALLOC_SIZE 1
#endif
#if __has_attribute(assume_aligned)
# define HAVE_ATTR_ASSUME_ALIGNED 1
#endif
#if __has_attribute(returns_nonnull)
# define HAVE_ATTR_RETURNS_NONNULL 1
#endif
#if __has_attribute(noinline)
# define HAVE_ATTR_NOINLINE 1
#endif
#if !defined(HAVE_BUILTIN_CLZ) && __has_builtin(__builtin_clz)
# define HAVE_BUILTIN_CLZ 1
#endif
#if !defined(HAVE_BUILTIN_EXPECT) && __has_builtin(__builtin_expect)
# define HAVE_BUILTIN_EXPECT 1
#endif
#if !defined(HAVE_BUILTIN_UNREACHABLE) && __has_builtin(__builtin_unreachable)
# define HAVE_BUILTIN_UNREACHABLE 1
#endif

#ifdef HAVE_ATTR_NONNULL
# define _attr_nonnull(...)                  __attribute__((nonnull (__VA_ARGS__)))
#else
# define _attr_nonnull(...)
#endif

#ifdef HAVE_ATTR_NODISCARD
# define _attr_nodiscard                     __attribute__((warn_unused_result))
#else
# define _attr_nodiscard
#endif

#ifdef HAVE_ATTR_ALWAYS_INLINE
# define _attr_always_inline                 inline __attribute__((always_inline))
#else
# define _attr_always_inline                 inline
#endif

#ifdef HAVE_ATTR_FALLTHROUGH
# define _attr_fallthrough                   __attribute__((fallthrough))
#else
# define _attr_fallthrough
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

#ifdef HAVE_ATTR_PACKED
# define _attr_packed                        __attribute__((packed))
// _attr_packed changes the generated code so don't define it if it is not available
#endif

#ifdef HAVE_ATTR_MALLOC
# define _attr_malloc                        __attribute__((malloc))
#else
# define _attr_malloc
#endif

#ifdef HAVE_ATTR_PURE
# define _attr_pure                          __attribute__((pure))
#else
# define _attr_pure
#endif

#ifdef HAVE_ATTR_CONST
# define _attr_const                         __attribute__((const))
#else
# define _attr_const
#endif

#ifdef HAVE_ATTR_ALLOC_SIZE
# define _attr_alloc_size(...)               __attribute__((alloc_size (__VA_ARGS__)))
#else
# define _attr_alloc_size(...)
#endif

#ifdef HAVE_ATTR_ASSUME_ALIGNED
# define _attr_assume_aligned(...)           __attribute__((assume_aligned (__VA_ARGS__)))
#else
# define _attr_assume_aligned(...)
#endif

#ifdef HAVE_ATTR_RETURNS_NONNULL
# define _attr_returns_nonnull               __attribute__((returns_nonnull))
#else
# define _attr_returns_nonnull
#endif

#ifdef HAVE_ATTR_NOINLINE
# define _attr_noinline                      __attribute__((noinline))
#else
# define _attr_noinline
#endif

#ifdef HAVE_BUILTIN_EXPECT
# define likely(expr)                        __builtin_expect(!!(expr), 1)
# define unlikely(expr)                      __builtin_expect(!!(expr), 0)
#else
# define likely(expr)                        (expr)
# define unlikely(expr)                      (expr)
#endif

#ifdef HAVE_BUILTIN_UNREACHABLE
# define unreachable()                       __builtin_unreachable()
#else
# define unreachable()                       do {} while (0)
#endif

#endif
