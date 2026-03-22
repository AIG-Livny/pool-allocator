# Pool Allocator & Multi Pool Allocator

A high-performance C allocator optimized for fixed-size data structures. The library includes a basic Pool Allocator and a MultiPool add-on for working with objects of varying sizes.

- O(1) Allocation & Deallocation: Memory allocation is simply a pointer swap on the free node stack.
- Minimal Fragmentation: Memory is allocated in large chunks (Arena-style), ensuring excellent cache locality.
- Thread-Safe: Optional multithreading support via mutexes and atomic operations (enabled by the USE_THREADSAFE macro).
- Linked Chunks: Using a linked list of chunks eliminates heavy realloc calls.
- Zero Overhead: Memory is reused for free nodes (intrusive list), and metadata does not take up unnecessary space.

## Components
1. Pool Allocator
Ideal when you need to create thousands of objects of the same type (e.g., RopeNode).

Fixed Size: All nodes are aligned on a pointer boundary.
LIFO Reuse: The last node freed is the first one reused, preserving the data in the processor's "hot" cache.

2. MultiPool (Size Classes)
A pool-based framework for efficient memory management of objects of different sizes (16, 32, 64... 2048 bytes).


🛠 Usage
Basic Pool
```c
#include "pool_allocator.h"

// Create a pool for your structure
pool_allocator *pool = pool_create(sizeof(MyStruct));

// Allocation
MyStruct *node = (MyStruct*)pool_alloc(pool);

// Deallocation (return to the free list)
pool_dealloc(pool, node);

// Complete memory cleanup
pool_destroy(pool);
```

MultiPool (Different Sizes)
```c

MultiPool *mp = multipool_create();

// The allocator will automatically select the appropriate Size Class (e.g., 64 bytes)
void *ptr = multipool_alloc(mp, 50);
void *ptr2 = multipool_alloc(mp, 120);

// Deallocate with specified size
multipool_dealloc(mp, ptr, 50);
multipool_dealloc(mp, ptr2, 120);

multipool_destroy(mp);
```
