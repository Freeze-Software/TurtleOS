#include <stdint.h>
#include <stddef.h>
#include "main.h"
#include "../kernel.h"

// ---------------- HEAP ----------------
static uint8_t heap[HEAP_SIZE];
static block_t* free_list = NULL;

// ---------------- ALIGN ----------------
static size_t align(size_t size) {
    return (size + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1);
}

// ---------------- INIT ----------------
void init_heap() {
    free_list = (block_t*)heap;
    free_list->size = HEAP_SIZE - sizeof(block_t);
    free_list->free = 1;
    free_list->next = NULL;
}

// ---------------- FIND BLOCK ----------------
static block_t* find_free_block(size_t size) {
    block_t* current = free_list;
    while (current) {
        if (current->free && current->size >= size) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

// ---------------- SPLIT BLOCK ----------------
static void split_block(block_t* block, size_t size) {
    if (block->size < size + sizeof(block_t) + ALIGNMENT)
        return;
    block_t* new_block =
        (block_t*)((uint8_t*)block + sizeof(block_t) + size);
    new_block->size = block->size - size - sizeof(block_t);
    new_block->free = 1;
    new_block->next = block->next;
    block->size = size;
    block->next = new_block;
}

// ---------------- COALESCE ----------------
static void coalesce() {
    block_t* current = free_list;
    while (current && current->next) {
        if (current->free && current->next->free) {
            current->size += sizeof(block_t) + current->next->size;
            current->next = current->next->next;
        } else {
            current = current->next;
        }
    }
}

// ---------------- IRQ SAVE/RESTORE ----------------
static inline unsigned long irq_save(void) {
    unsigned long flags;
    __asm__ volatile("pushf; pop %0; cli" : "=r"(flags) :: "memory");
    return flags;
}

static inline void irq_restore(unsigned long flags) {
    __asm__ volatile("push %0; popf" :: "r"(flags) : "memory", "cc");
}

// ---------------- MALLOC ----------------
void* malloc(size_t size) {
    if (size == 0) return NULL;
    size = align(size);

    unsigned long flags = irq_save();

    block_t* block = find_free_block(size);
    if (!block) {
        irq_restore(flags);
        return NULL;
    }
    if (block->size >= size + sizeof(block_t) + ALIGNMENT) {
        split_block(block, size);
    }
    block->free = 0;

    irq_restore(flags);
    return (void*)((uint8_t*)block + sizeof(block_t));
}

// ---------------- FREE ----------------
void free(void* ptr) {
    if (!ptr) return;

    unsigned long flags = irq_save();

    block_t* block =
        (block_t*)((uint8_t*)ptr - sizeof(block_t));
    block->free = 1;
    coalesce();

    irq_restore(flags);
}

// ---------------- REALLOC ----------------
void* realloc(void* ptr, size_t size) {
    if (!ptr) return malloc(size);
    if (size == 0) {
        free(ptr);
        return NULL;
    }

    unsigned long flags = irq_save();
    block_t* block =
        (block_t*)((uint8_t*)ptr - sizeof(block_t));
    size_t old_size = block->size;
    irq_restore(flags);

    if (old_size >= size) {
        return ptr;
    }

    void* new_ptr = malloc(size);
    if (!new_ptr) return NULL;

    size_t copy_size = old_size < size ? old_size : size;
    for (size_t i = 0; i < copy_size; i++) {
        ((uint8_t*)new_ptr)[i] = ((uint8_t*)ptr)[i];
    }

    free(ptr);
    return new_ptr;
}

heap_stats_t heap_get_stats(void) {
    heap_stats_t stats = {0, 0, 0};
    stats.total = HEAP_SIZE;

    unsigned long flags = irq_save();

    int count = 0;
    for (block_t* b = free_list; b != NULL; b = b->next) {
        count++;
        console_writefln(
            "block %d: addr=%x size=%d free=%d next=%x",
            count,
            b,
            b->size,
            b->free,
            b->next
        );
        if (b->free)
            stats.free += b->size;
        else
            stats.used += b->size;
    }
    console_writefln("blocks total: %d", count);

    irq_restore(flags);
    return stats;
}
