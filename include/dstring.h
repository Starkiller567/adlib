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

#ifndef __DSTRING_INCLUDE__
#define __DSTRING_INCLUDE__

// TODO join/concat/append with varargs, upper/lower?, casecmp?
// TODO rename dstr_add/insert/replace to dstr_*_dstr?

#include <stdarg.h> // va_list
#include <stdbool.h> // bool
#include <stddef.h> // size_t
#include "macros.h"

#define STRVIEW_NPOS ((size_t)-1)
#define DSTR_NPOS    STRVIEW_NPOS

typedef char * dstr_t;

struct strview {
	const char *characters;
	size_t length;
};

struct dstr_list {
	dstr_t *strings;
	size_t count;
};

struct strview_list {
	struct strview *strings;
	size_t count;
};

__AD_LINKAGE _attr_unused _attr_warn_unused_result dstr_t dstr_new_empty(void);
__AD_LINKAGE _attr_unused _attr_warn_unused_result dstr_t dstr_from_chars(const char *chars, size_t n);
__AD_LINKAGE _attr_unused _attr_warn_unused_result dstr_t dstr_from_cstr(const char *cstr);
__AD_LINKAGE _attr_unused _attr_warn_unused_result dstr_t dstr_from_view(struct strview view);
__AD_LINKAGE _attr_unused _attr_warn_unused_result _attr_format_printf(1, 2) dstr_t dstr_from_fmt(const char *fmt, ...);
__AD_LINKAGE _attr_unused _attr_warn_unused_result _attr_format_printf(1, 0) dstr_t dstr_from_fmtv(const char *fmt, va_list args);
__AD_LINKAGE _attr_unused size_t dstr_length(const dstr_t dstr);
__AD_LINKAGE _attr_unused bool dstr_empty(const dstr_t dstr); // TODO rename dstr_is_empty?
__AD_LINKAGE _attr_unused size_t dstr_capacity(const dstr_t dstr);
__AD_LINKAGE _attr_unused void dstr_resize(dstr_t *dstrp, size_t new_capacity);
__AD_LINKAGE _attr_unused void dstr_free(dstr_t *dstrp);
__AD_LINKAGE _attr_unused void dstr_reserve(dstr_t *dstrp, size_t n);
__AD_LINKAGE _attr_unused void dstr_clear(dstr_t *dstrp);
__AD_LINKAGE _attr_unused void dstr_shrink_to_fit(dstr_t *dstrp);
__AD_LINKAGE _attr_unused _attr_warn_unused_result dstr_t dstr_copy(const dstr_t dstr);
__AD_LINKAGE _attr_unused void dstr_append_char(dstr_t *dstrp, char c);
__AD_LINKAGE _attr_unused void dstr_append_chars(dstr_t *dstrp, const char *chars, size_t n);
__AD_LINKAGE _attr_unused void dstr_append(dstr_t *dstrp, const dstr_t dstr);
__AD_LINKAGE _attr_unused void dstr_append_cstr(dstr_t *dstrp, const char *cstr);
__AD_LINKAGE _attr_unused void dstr_append_view(dstr_t *dstrp, struct strview view);
__AD_LINKAGE _attr_unused _attr_format_printf(2, 3) size_t dstr_append_fmt(dstr_t *dstrp, const char *fmt, ...);
__AD_LINKAGE _attr_unused _attr_format_printf(2, 0) size_t dstr_append_fmtv(dstr_t *dstrp, const char *fmt, va_list args);
__AD_LINKAGE _attr_unused void dstr_insert_char(dstr_t *dstrp, size_t pos, char c);
__AD_LINKAGE _attr_unused void dstr_insert_chars(dstr_t *dstrp, size_t pos, const char *chars, size_t n);
__AD_LINKAGE _attr_unused void dstr_insert(dstr_t *dstrp, size_t pos, const dstr_t dstr);
__AD_LINKAGE _attr_unused void dstr_insert_cstr(dstr_t *dstrp, size_t pos, const char *cstr);
__AD_LINKAGE _attr_unused void dstr_insert_view(dstr_t *dstrp, size_t pos, struct strview view);
__AD_LINKAGE _attr_unused _attr_format_printf(3, 4) size_t dstr_insert_fmt(dstr_t *dstrp, size_t pos, const char *fmt, ...);
__AD_LINKAGE _attr_unused _attr_format_printf(3, 0) size_t dstr_insert_fmtv(dstr_t *dstrp, size_t pos, const char *fmt, va_list args);
__AD_LINKAGE _attr_unused void dstr_replace_chars(dstr_t *dstrp, size_t pos, size_t len, const char *chars, size_t n);
__AD_LINKAGE _attr_unused void dstr_replace(dstr_t *dstrp, size_t pos, size_t len, const dstr_t dstr);
__AD_LINKAGE _attr_unused void dstr_replace_cstr(dstr_t *dstrp, size_t pos, size_t len, const char *cstr);
__AD_LINKAGE _attr_unused void dstr_replace_view(dstr_t *dstrp, size_t pos, size_t len, struct strview view);
__AD_LINKAGE _attr_unused _attr_format_printf(4, 5) size_t dstr_replace_fmt(dstr_t *dstrp, size_t pos, size_t len, const char *fmt, ...);
__AD_LINKAGE _attr_unused _attr_format_printf(4, 0) size_t dstr_replace_fmtv(dstr_t *dstrp, size_t pos, size_t len, const char *fmt, va_list args);
__AD_LINKAGE _attr_unused void dstr_erase(dstr_t *dstrp, size_t pos, size_t len);
__AD_LINKAGE _attr_unused void dstr_strip(dstr_t *dstrp, const char *strip);
__AD_LINKAGE _attr_unused void dstr_lstrip(dstr_t *dstrp, const char *strip);
__AD_LINKAGE _attr_unused void dstr_rstrip(dstr_t *dstrp, const char *strip);
__AD_LINKAGE _attr_unused _attr_warn_unused_result char *dstr_to_cstr(dstr_t *dstrp);
__AD_LINKAGE _attr_unused _attr_warn_unused_result char *dstr_to_cstr_copy(const dstr_t dstr);
__AD_LINKAGE struct strview dstr_view(const dstr_t dstr);
__AD_LINKAGE _attr_unused struct strview dstr_substring_view(const dstr_t dstr, size_t start, size_t length);
__AD_LINKAGE _attr_unused void dstr_substring(dstr_t *dstrp, size_t start, size_t length);
__AD_LINKAGE _attr_unused _attr_warn_unused_result dstr_t dstr_substring_copy(const dstr_t dstr, size_t start, size_t length);
__AD_LINKAGE _attr_unused int dstr_compare(const dstr_t dstr1, const dstr_t dstr2);
__AD_LINKAGE _attr_unused int dstr_compare_view(const dstr_t dstr, struct strview view);
__AD_LINKAGE _attr_unused int dstr_compare_cstr(const dstr_t dstr, const char *cstr);
__AD_LINKAGE _attr_unused bool dstr_equal(const dstr_t dstr1, const dstr_t dstr2);
__AD_LINKAGE _attr_unused bool dstr_equal_view(const dstr_t dstr, struct strview view);
__AD_LINKAGE _attr_unused bool dstr_equal_cstr(const dstr_t dstr, const char *cstr);
__AD_LINKAGE _attr_unused size_t dstr_find(const dstr_t haystack, const dstr_t needle, size_t pos);
__AD_LINKAGE _attr_unused size_t dstr_find_view(const dstr_t haystack, struct strview needle_view, size_t pos);
__AD_LINKAGE _attr_unused size_t dstr_find_cstr(const dstr_t haystack, const char *needle_cstr, size_t pos);
__AD_LINKAGE _attr_unused size_t dstr_rfind(const dstr_t haystack, const dstr_t needle, size_t pos);
__AD_LINKAGE _attr_unused size_t dstr_rfind_view(const dstr_t haystack, struct strview needle_view, size_t pos);
__AD_LINKAGE _attr_unused size_t dstr_rfind_cstr(const dstr_t haystack, const char *needle_cstr, size_t pos);
// TODO should these have a pos argument
__AD_LINKAGE _attr_unused size_t dstr_find_replace(dstr_t *haystackp, const dstr_t needle, const dstr_t dstr, size_t max);
__AD_LINKAGE _attr_unused size_t dstr_find_replace_view(dstr_t *haystackp, struct strview needle_view, struct strview view, size_t max);
__AD_LINKAGE _attr_unused size_t dstr_find_replace_cstr(dstr_t *haystackp, const char *needle_cstr, const char *cstr, size_t max);
__AD_LINKAGE _attr_unused size_t dstr_rfind_replace(dstr_t *haystackp, const dstr_t needle, const dstr_t dstr, size_t max);
__AD_LINKAGE _attr_unused size_t dstr_rfind_replace_view(dstr_t *haystackp, struct strview needle_view, struct strview view, size_t max);
__AD_LINKAGE _attr_unused size_t dstr_rfind_replace_cstr(dstr_t *haystackp, const char *needle_cstr, const char *cstr, size_t max);
__AD_LINKAGE _attr_unused size_t dstr_find_first_of(const dstr_t dstr, const char *accept, size_t pos);
__AD_LINKAGE _attr_unused size_t dstr_find_last_of(const dstr_t dstr, const char *accept, size_t pos);
__AD_LINKAGE _attr_unused size_t dstr_find_first_not_of(const dstr_t dstr, const char *reject, size_t pos);
__AD_LINKAGE _attr_unused size_t dstr_find_last_not_of(const dstr_t dstr, const char *reject, size_t pos);
__AD_LINKAGE _attr_unused bool dstr_startswith(const dstr_t dstr, const dstr_t prefix);
__AD_LINKAGE _attr_unused bool dstr_startswith_view(const dstr_t dstr, struct strview prefix);
__AD_LINKAGE _attr_unused bool dstr_startswith_cstr(const dstr_t dstr, const char *prefix);
__AD_LINKAGE _attr_unused bool dstr_endswith(const dstr_t dstr, const dstr_t suffix);
__AD_LINKAGE _attr_unused bool dstr_endswith_view(const dstr_t dstr, struct strview suffix);
__AD_LINKAGE _attr_unused bool dstr_endswith_cstr(const dstr_t dstr, const char *suffix);
__AD_LINKAGE _attr_unused _attr_warn_unused_result struct dstr_list dstr_split(const dstr_t dstr, char c, size_t max);
__AD_LINKAGE _attr_unused _attr_warn_unused_result struct dstr_list dstr_rsplit(const dstr_t dstr, char c, size_t max);
__AD_LINKAGE _attr_unused _attr_warn_unused_result struct strview_list dstr_split_views(const dstr_t dstr, char c, size_t max);
__AD_LINKAGE _attr_unused _attr_warn_unused_result struct strview_list dstr_rsplit_views(const dstr_t dstr, char c, size_t max);
__AD_LINKAGE _attr_unused void dstr_list_free(struct dstr_list *list);


__AD_LINKAGE _attr_unused struct strview strview_from_chars(const char *chars, size_t n);
__AD_LINKAGE _attr_unused struct strview strview_from_cstr(const char *cstr);
__AD_LINKAGE _attr_unused _attr_warn_unused_result char *strview_to_cstr(struct strview view);
__AD_LINKAGE _attr_unused struct strview strview_substring(struct strview view, size_t start, size_t length);
__AD_LINKAGE _attr_unused struct strview strview_narrow(struct strview view, size_t left, size_t right);
__AD_LINKAGE _attr_unused int strview_compare(struct strview view1, struct strview view2);
__AD_LINKAGE _attr_unused int strview_compare_cstr(struct strview view, const char *cstr);
__AD_LINKAGE _attr_unused bool strview_equal(struct strview view1, struct strview view2);
__AD_LINKAGE _attr_unused bool strview_equal_cstr(struct strview view, const char *cstr);
__AD_LINKAGE _attr_unused size_t strview_find(struct strview haystack, struct strview needle, size_t pos);
__AD_LINKAGE _attr_unused size_t strview_find_cstr(struct strview haystack, const char *needle, size_t pos);
__AD_LINKAGE _attr_unused size_t strview_rfind(struct strview haystack, struct strview needle, size_t pos);
__AD_LINKAGE _attr_unused size_t strview_rfind_cstr(struct strview haystack, const char *needle, size_t pos);
__AD_LINKAGE _attr_unused size_t strview_find_first_of(struct strview view, const char *accept, size_t pos);
__AD_LINKAGE _attr_unused size_t strview_find_last_of(struct strview view, const char *accept, size_t pos);
__AD_LINKAGE _attr_unused size_t strview_find_first_not_of(struct strview view, const char *reject, size_t pos);
__AD_LINKAGE _attr_unused size_t strview_find_last_not_of(struct strview view, const char *reject, size_t pos);
__AD_LINKAGE _attr_unused struct strview strview_strip(struct strview view, const char *strip);
__AD_LINKAGE _attr_unused struct strview strview_lstrip(struct strview view, const char *strip);
__AD_LINKAGE _attr_unused struct strview strview_rstrip(struct strview view, const char *strip);
__AD_LINKAGE _attr_unused bool strview_startswith(struct strview view, struct strview prefix);
__AD_LINKAGE _attr_unused bool strview_startswith_cstr(struct strview view, const char *prefix);
__AD_LINKAGE _attr_unused bool strview_endswith(struct strview view, struct strview suffix);
__AD_LINKAGE _attr_unused bool strview_endswith_cstr(struct strview view, const char *suffix);
__AD_LINKAGE _attr_unused _attr_warn_unused_result struct strview_list strview_split(struct strview view, char c, size_t max);
__AD_LINKAGE _attr_unused _attr_warn_unused_result struct strview_list strview_rsplit(struct strview view, char c, size_t max);
__AD_LINKAGE _attr_unused void strview_list_free(struct strview_list *list);


__AD_LINKAGE _attr_unused void *_dstr_debug_get_head_ptr(const dstr_t dstr);

#endif
