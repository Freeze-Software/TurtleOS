//Simple header for main .c file
#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>
#include <stdint.h>

#define HEAP_SIZE (1024 * 10240 * 4)
#define ALIGNMENT 8

typedef struct block {
    size_t size;
    int free;
    struct block* next;
} __attribute__((aligned(8))) block_t;

typedef struct {
    size_t total;
    size_t used;
    size_t free;
} heap_stats_t;

void init_heap(void);

void* malloc(size_t size);
void  free(void* ptr);
void* realloc(void* ptr, size_t size);
heap_stats_t heap_get_stats();

#endif // MEMORY_H
