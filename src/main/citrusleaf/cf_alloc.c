/* 
 * Copyright 2008-2018 Aerospike, Inc.
 *
 * Portions may be licensed to Aerospike, Inc. under one or more contributor
 * license agreements.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy of
 * the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations under
 * the License.
 */
 
/*
 *  NB:  Compile this default memory allocator only if the enhanced memory allocator is *NOT* enabled.
 */
#ifndef ENHANCED_ALLOC

#include <citrusleaf/alloc.h>
#include <aerospike/as_atomic.h>
#include <stdlib.h>
#include <string.h>

void*
trace_alloc(size_t sz, void *ptr)
{
    return ptr;
}

void
trace_free(void *ptr)
{
}

void*
cf_malloc(size_t sz)
{
	return trace_alloc(sz, malloc(sz));
}

void*
cf_calloc(size_t nmemb, size_t sz)
{
	return trace_alloc(nmemb * sz, calloc(nmemb, sz));
}

void*
cf_realloc(void *ptr, size_t sz)
{
    trace_free(ptr);
	return trace_alloc(sz, realloc(ptr,sz));
}

void*
cf_strdup(const char *s)
{
	return trace_alloc(strlen(s), strdup(s));
}

void*
cf_strndup(const char *s, size_t n)
{
#if defined(_MSC_VER)
	size_t len = strnlen(s, n);
	char* t = cf_malloc(len + 1);

	if (t == NULL) {
		return NULL;
	}
	t[len] = 0;
	return memcpy(t, s, len);
#else
	return trace_alloc(strnlen(s, n), strndup(s, n));
#endif
}

void*
cf_valloc(size_t sz)
{
#if defined(_MSC_VER) || defined(__FreeBSD__)
	// valloc is not used by the client.
	// Since this file is for the client only, just return null.
	return NULL;
#else
	return trace_alloc(sz, valloc(sz));
#endif
}

void
cf_free(void *p)
{
    trace_free(p);
	free(p);
}

uint32_t
cf_rc_reserve(void* addr)
{
	cf_rc_hdr* head = (cf_rc_hdr*)addr - 1;
	return as_aaf_uint32(&head->count, 1);
}

void*
cf_rc_alloc(size_t sz)
{
	cf_rc_hdr* head = cf_malloc(sizeof(cf_rc_hdr) + sz);

	head->count = 1;
	head->sz = (uint32_t)sz;

	return head + 1;
}

void
cf_rc_free(void* addr)
{
	cf_rc_hdr* head = (cf_rc_hdr*)addr - 1;
    trace_free(head);
	free(head);
}

uint32_t
cf_rc_release(void* addr)
{
	cf_rc_hdr* head = (cf_rc_hdr*)addr - 1;
	uint32_t rc = as_aaf_uint32_rls(&head->count, -1);

	if (rc == 0) {
		// Subsequent destructor may require an 'acquire' barrier.
		as_fence_acq();
	}

	return rc;
}

uint32_t
cf_rc_releaseandfree(void* addr)
{
	cf_rc_hdr* head = (cf_rc_hdr*)addr - 1;
	uint32_t rc = as_aaf_uint32_rls(&head->count, -1);

	if (rc == 0) {
        trace_free(head);
		free(head);
	}

	return rc;
}

#endif // defined(ENHANCED_ALLOC)
