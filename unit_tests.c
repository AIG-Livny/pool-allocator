#include "utest.h"
#include <stdarg.h>
#include <stdlib.h>
#include "cvector.h"
#include "stdbool.h"
#include <iso646.h>
#include <pthread.h>

typedef struct Allocation{
    void* address;
    size_t size;
}Allocation;

cvector(Allocation) allocations;

void print_allocations(const char* where){
    printf("(%s) Allocations count=%lu\n",where,cvector_size(allocations));
    cvector_iterator(Allocation) it;
    cvector_for_each_in(it, allocations) {
        printf("Allocation %lu size=%lu address=0x%lx\n", it-allocations, it->size, (long)it->address);
    }
}

void* test_malloc(size_t size){
    void* addr = malloc(size);
    cvector_push_back(allocations,(
    (Allocation){
        .address = addr,
        .size = size,
    }));

    return addr;
}

void test_free(void* ptr) {
    cvector_iterator(Allocation) it;
    bool found = false;
    cvector_for_each_in(it, allocations) {
        if ( it->address == ptr ) {
            cvector_erase(allocations, it - allocations);
            found = true;
            break;
        }
    }

    if ( not found ){
        printf("ERROR\n");
        print_allocations(__FUNCTION__);
    }

    free(ptr);
}


typedef struct mystruct {
    uint32_t myvalue;
} mystruct;

#define POOL_MALLOC test_malloc
#define POOL_FREE test_free
#define POOL_CHUNK_SIZE 100

#define DEBUG
#define USE_THREADSAFE

#include "pool_allocator.h"

pool_allocator* pool;

void destructor(void* ptr){
    pool_dealloc(pool,ptr);
}

UTEST(test, pool_alloc_dealloc) {
    pool = pool_create(sizeof(mystruct));
    cvector(mystruct*) vec=NULL;
    cvector_init(vec,10,destructor);

    for ( int i =0; i < 40; i++ ){
        cvector_push_back(vec, pool_alloc(pool));
    }

    int start_allocations_num = cvector_size(allocations);

    // Deallocate half
    for ( int i =0; i < 20; i++ ){
        cvector_pop_back(vec);
    }

    // Allocate half again
    for ( int i =0; i < 20; i++ ){
        cvector_push_back(vec, pool_alloc(pool));
    }

    // Allocations must not change
    ASSERT_EQ(cvector_size(allocations), start_allocations_num);

    pool_destroy(pool);

    // All memory free
    ASSERT_EQ(cvector_size(allocations), 0);
}

UTEST(test, pool_alloc_dealloc_many) {
    pool = pool_create(sizeof(mystruct));

    cvector(mystruct*) vec=NULL;
    cvector_init(vec,10000,destructor);
    cvector_iterator(mystruct*) it;

    for ( int i =0; i < 10000; i++ ){
        cvector_push_back(vec,pool_alloc(pool));
    }

    int start_allocations_num = cvector_size(allocations);

    // deallocate part
    cvector_erase_range(vec,4999,cvector_size(vec)-1);

    // allocate again
    for ( int i =0; i < 5000; i++ ){
        cvector_push_back(vec,pool_alloc(pool));
    }

    // Allocations must not change
    ASSERT_EQ(cvector_size(allocations), start_allocations_num);

    pool_destroy(pool);

    // All memory free
    ASSERT_EQ(cvector_size(allocations), 0);
}

#define THREADS_COUNT 8
#define ITERATIONS 5000

typedef struct {
    uint32_t thread_id;
    uint32_t counter;
} test_data;

void* thread_func(void* arg) {
    pool_allocator *p = (pool_allocator*)arg;
    uint32_t tid = (uint32_t)pthread_self();
    test_data* nodes[ITERATIONS];

    for (int i = 0; i < ITERATIONS; i++) {
        nodes[i] = (test_data*)pool_alloc(p);
        nodes[i]->thread_id = tid;
        nodes[i]->counter = i;
    }

    usleep(10);

    for (int i = 0; i < ITERATIONS; i++) {
        if (nodes[i]->thread_id != tid) {
            printf("RACE CONDITION DETECTED! Thread %u sees %u\n", tid, nodes[i]->thread_id);
        }
        pool_dealloc(p, nodes[i]);
    }

    return NULL;
}

UTEST(test, pool_multithreaded_stress) {
    pool = pool_create(sizeof(test_data));
    pthread_t threads[THREADS_COUNT];

    for (int i = 0; i < THREADS_COUNT; i++) {
        pthread_create(&threads[i], NULL, thread_func, pool);
    }

    for (int i = 0; i < THREADS_COUNT; i++) {
        pthread_join(threads[i], NULL);
    }

    pool_destroy(pool);
}

multipool_allocator *mp;

void multipool_destructor64(void* ptr){
    multipool_dealloc(mp,64,ptr);
}

void multipool_destructor32(void* ptr){
    multipool_dealloc(mp,32,ptr);
}

UTEST(test, multipool_allocation_economy) {
    cvector(mystruct*) vec64=NULL;
    cvector(mystruct*) vec32=NULL;
    cvector_init(vec64,100,multipool_destructor64);
    cvector_init(vec32,100,multipool_destructor32);

    mp = multipool_create();

    int base_allocs = cvector_size(allocations);

    for (int i = 0; i < 100; i++) {
        cvector_push_back(vec64,multipool_alloc(mp, 64));
        cvector_push_back(vec32,multipool_alloc(mp, 32));
    }

    int allocs_after_fill = cvector_size(allocations);
    ASSERT_GT(allocs_after_fill, base_allocs);

    // Free all
    cvector_clear(vec64);
    cvector_clear(vec32);

    // repeat
    for (int i = 0; i < 100; i++) {
        cvector_push_back(vec64,multipool_alloc(mp, 64));
        cvector_push_back(vec32,multipool_alloc(mp, 32));
    }

    // We not expect new allocations
    ASSERT_EQ(cvector_size(allocations), allocs_after_fill);

    multipool_destroy(mp);

    ASSERT_EQ(cvector_size(allocations), 0);
}

UTEST_MAIN();