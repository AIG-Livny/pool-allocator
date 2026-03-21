#ifndef POOL_ALLOCATOR_H
#define POOL_ALLOCATOR_H

#include <stdlib.h>
#include <stdint.h>

#ifndef POOL_MALLOC
#define POOL_MALLOC malloc
#endif

#ifndef POOL_CALLOC
#define POOL_CALLOC calloc
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

    pool_allocator *p = POOL_CALLOC(1, sizeof(pool_allocator));
    p->node_size = aligned_size;

    POOL_INIT_LOCK(p);
    return p;
}

static inline bool pool_expand_unsafe(pool_allocator* p) {
    uint8_t* new_chunk = POOL_MALLOC(POOL_CHUNK_SIZE);
    if (!new_chunk) return false;

    // First sizeof(void*) bytes of chunk will keep address previos chunk
    *(void**)new_chunk = p->chunks_head;
    p->chunks_head = new_chunk;

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
    if (!ptr) return;

    pool_empty_node* node = (pool_empty_node*)ptr;

    POOL_LOCK(p);
    node->next = p->empty_list;
    p->empty_list = node;
    POOL_UNLOCK(p);
}

static inline void pool_destroy(pool_allocator* p) {
    void *current = p->chunks_head;
    while (current) {
        void *next = *(void**)current;
        POOL_FREE(current);
        current = next;
    }

    POOL_DESTROY_LOCK(p);
    POOL_FREE(p);
}

#endif // POOL_ALLOCATOR_H