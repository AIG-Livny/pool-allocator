/*
 * pool_allocator
 * Copyright (C) 2026 Ivan Agibalov (AIG) (aig.livny@gmail.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */


#ifndef POOL_ALLOCATOR_H
#define POOL_ALLOCATOR_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifndef POOL_MALLOC
#define POOL_MALLOC malloc
#endif

#ifndef POOL_FREE
#define POOL_FREE free
#endif

#if defined(_PTHREAD_H) && !defined(USE_THREADSAFE)
    #warning "_PTHREAD_H defined, but not USE_THREADSAFE"
#endif

#ifdef USE_THREADSAFE
    #include <pthread.h>
    #define POOL_LOCK(p)   pthread_mutex_lock(&(p)->lock)
    #define POOL_UNLOCK(p) pthread_mutex_unlock(&(p)->lock)
    #define POOL_INIT_LOCK(p) pthread_mutex_init(&(p)->lock, NULL)
    #define POOL_DESTROY_LOCK(p) pthread_mutex_destroy(&(p)->lock)
#else
    #define POOL_LOCK(p)
    #define POOL_UNLOCK(p)
    #define POOL_INIT_LOCK(p)
    #define POOL_DESTROY_LOCK(p)
#endif

#ifndef POOL_CHUNK_SIZE
#define POOL_CHUNK_SIZE (1024 * 64) // 64kb chunks
#endif

typedef struct pool_empty_node_t {
    struct pool_empty_node_t *next;
} pool_empty_node;

// Empty nodes stores like a stack FIFO
typedef struct {
    size_t node_size;
    pool_empty_node* empty_list;
    void* chunks_head;
    #ifdef USE_THREADSAFE
    pthread_mutex_t lock;
    #endif
} pool_allocator;

static inline pool_allocator* pool_create(size_t node_size) {
    // Align size by pointer size
    size_t aligned_size = (node_size + sizeof(void*) - 1) & ~(sizeof(void*) - 1);

    pool_allocator *p = POOL_MALLOC(sizeof(pool_allocator));
    p->node_size = aligned_size;
    p->chunks_head = NULL;
    p->empty_list = NULL;

    POOL_INIT_LOCK(p);
    return p;
}

static inline bool pool_expand_unsafe(pool_allocator* p) {
    uint8_t* new_chunk = POOL_MALLOC(POOL_CHUNK_SIZE);
    if (!new_chunk) return false;

    // First sizeof(void*) bytes of chunk will keep address previous chunk
    *(void**)new_chunk = p->chunks_head;
    p->chunks_head = new_chunk;

    // Cut the chunk on nodes and store them in empty_list
    size_t offset = sizeof(void*);
    while (offset + p->node_size <= POOL_CHUNK_SIZE) {
        pool_empty_node* node = (pool_empty_node *)(new_chunk + offset);
        node->next = p->empty_list;
        p->empty_list = node;
        offset += p->node_size;
    }
    return true;
}

static inline void* pool_alloc(pool_allocator* p) {
    POOL_LOCK(p);

    if (!p->empty_list) {
        pool_expand_unsafe(p);
    }

    pool_empty_node* node = p->empty_list;
    if (node) {
        p->empty_list = node->next;
    }

    POOL_UNLOCK(p);

    return node;
}

static inline void pool_dealloc(pool_allocator* p, void* ptr) {
    if ( ptr == NULL) {
        return;
    }

    // We not really dealocate the node, instead we place it in free list
    pool_empty_node* node = (pool_empty_node*)ptr;

    POOL_LOCK(p);
    node->next = p->empty_list;
    p->empty_list = node;
    POOL_UNLOCK(p);
}

static inline void pool_destroy(pool_allocator* p) {
    if ( p == NULL ) {
        return;
    }

    void *current = p->chunks_head;
    while (current) {
        void *next = *(void**)current;
        POOL_FREE(current);
        current = next;
    }

    POOL_DESTROY_LOCK(p);
    POOL_FREE(p);
}

/*
    Multi pool - pool for different sizes
*/

// Class is data size in power of 2. 1 - 32, 2 - 64, 3 - 128 ...
// This number defines how many pools we will store for different sizes
// Minimal size - 16 bytes blocks
#ifndef MULTIPOOL_MAX_CLASS
#define MULTIPOOL_MAX_CLASS 8
#endif

typedef struct {
    pool_allocator* size_classes[MULTIPOOL_MAX_CLASS];
} multipool_allocator;

static inline multipool_allocator* multipool_create() {
    multipool_allocator *p = POOL_MALLOC(sizeof(multipool_allocator));
    memset(p,0,sizeof(multipool_allocator));
    return p;
}

// Find nearest power of 2 (index in size_classes)
static inline int multipool_get_pool_index(size_t size) {
    if (size <= 16) return 0;
    return 32 - __builtin_clz((uint32_t)size - 1) - 4;
}

static inline void* multipool_alloc(multipool_allocator* mp, size_t size) {
    int idx = multipool_get_pool_index(size);
    if (idx >= MULTIPOOL_MAX_CLASS) {
        return POOL_MALLOC(size); // Too big for us, send it to usual malloc
    }

    if(mp->size_classes[idx] == NULL) {
        size_t exact_size = 16;
        for(int i=0; i<idx; i++){ exact_size *= 2; }
        mp->size_classes[idx] = pool_create(exact_size);
    }

    return pool_alloc(mp->size_classes[idx]);
}

static inline void multipool_dealloc(multipool_allocator* mp, size_t size, void* ptr) {
    int idx = multipool_get_pool_index(size);
    if (idx >= MULTIPOOL_MAX_CLASS) {
        POOL_FREE(ptr); // Too big for us, maybe it been allocated by malloc
        return;
    }

    if ( mp->size_classes[idx] != NULL ) {
        pool_dealloc(mp->size_classes[idx], ptr);
    }
}

static inline void multipool_destroy(multipool_allocator* mp) {
    if ( mp == NULL ) {
        return;
    }

    for( int i = 0; i < MULTIPOOL_MAX_CLASS; i++ ) {
        pool_destroy(mp->size_classes[i]);
    }
    POOL_FREE(mp);
}

#endif // POOL_ALLOCATOR_H