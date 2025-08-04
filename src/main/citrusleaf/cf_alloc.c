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
#include <stdatomic.h>

atomic_size_t allocated;

size_t cf_allocated()
{
    return allocated;
}

void*
cf_malloc(size_t sz)
{
    sz += sizeof(size_t);
    void *p = malloc(sz);
    *(size_t *)p = sz;
    atomic_fetch_add(&allocated, sz);
    return (char *)p + sizeof(size_t);
}

void*
cf_calloc(size_t nmemb, size_t sz)
{
    return cf_malloc(nmemb * sz);
}

void*
cf_realloc(void *ptr, size_t sz)
{
    if (ptr == NULL) return cf_malloc(sz);
    void *p = (char *)ptr - sizeof(size_t);
    size_t sz_ = *(size_t *)p;
    p = realloc(p, sz + sizeof(size_t));
    *(size_t *)p = sz;
    atomic_fetch_add(&allocated, sz - sz_);
    return (char *)p + sizeof(size_t);
}

void*
cf_strdup(const char *s)
{
    size_t n = strlen(s);
    void *p = cf_malloc(n + 1);
    if (p == NULL) return NULL;
    return strcpy(p, s);
}

void*
cf_strndup(const char *s, size_t n)
{
    size_t l = strlen(s);
    if (l < n) n = l;
    void *p = cf_malloc(n + 1);
    if (p == NULL) return NULL;
    return strncpy(p, s, n);
}

void*
cf_valloc(size_t sz)
{
    // valloc is not used by the client.
    // Since this file is for the client only, just return null.
    return NULL;
}

void
cf_free(void *ptr)
{
    if (ptr == NULL) return;
    void *p = (char *)ptr - sizeof(size_t);
    size_t sz = *(size_t *)p;
    atomic_fetch_sub(&allocated, sz);
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
    cf_free(head);
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
        cf_free(head);
    }

    return rc;
}

#endif // defined(ENHANCED_ALLOC)
