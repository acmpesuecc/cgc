#include "gc.h"
#include <stdio.h>
#include <sys/mman.h>
#include <stdlib.h>

#define PAGE_SIZE 4096
#define ALLOC_SUCCESS 0
#define ALLOC_FAILURE 1
#ifndef ALLOC_LIMIT  // Prevent redefinition
#define ALLOC_LIMIT (100 * PAGE_SIZE)  

typedef struct mem_block {  // Renamed to avoid conflicts
    size_t size;
    struct mem_block *next;
    struct mem_block *prev;
    int marked;
} mem_block_t;

typedef struct allocator {  // Renamed to avoid conflicts
    mem_block_t *free_list;
    mem_block_t *used_list;
    void *bos;  // Bottom of the stack
    size_t allocated_size;
} allocator_t;

// Fetch memory pages using mmap
static void *mempage_fetch(unsigned int num_pages) {
    void *block = mmap(NULL, num_pages * PAGE_SIZE, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (block == MAP_FAILED) {
        return NULL;
    }
    return block;
}

u_int16_t add_to_free_list(allocator_t *allocator, mem_block_t *block) {
    mem_block_t *free_list = allocator->free_list;

    while (!(block > free_list && block < free_list->next)) {
        if (free_list >= free_list->next && (block > free_list || block < free_list->next)) {
            break;
        }
        free_list = free_list->next;
    }

    if ((char *)block + block->size == (char *)free_list->next) {
        block->size += free_list->next->size;
        block->next = free_list->next->next;
        if (free_list->next->next != NULL) {
            free_list->next->next->prev = block;
        }
    } else {
        block->next = free_list->next;
        free_list->next->prev = block;
    }

    if ((char *)free_list + free_list->size == (char *)block) {
        free_list->size += block->size;
        free_list->next = block->next;
        if (block->next != NULL) {
            block->next->prev = free_list;
        }
    } else {
        free_list->next = block;
    }

    block->prev = free_list;
    return ALLOC_SUCCESS;
}

// Initialize allocator and memory lists
allocator_t *alloc_init() {
    // Calculate total pages needed for allocator and structure
    size_t total_pages = (sizeof(allocator_t) + 2 * sizeof(mem_block_t) + PAGE_SIZE - 1) / PAGE_SIZE;
    void *mem_block = mempage_fetch(total_pages);
    if (mem_block == NULL) {
        return NULL;
    }

    // Initialize allocator structure
    allocator_t *allocator = (allocator_t *)mem_block;
    allocator->allocated_size = 0;
    allocator->bos = __builtin_frame_address(0);  // Set bottom of stack

    // Allocate space for free_list and used_list within the same memory block
    mem_block_t *free_list = (mem_block_t *)((char *)mem_block + sizeof(allocator_t));
    mem_block_t *used_list = (mem_block_t *)((char *)free_list + sizeof(mem_block_t));

    // Initialize free_list
    free_list->marked = 1;
    free_list->next = free_list;
    free_list->prev = free_list;
    free_list->size = 0;

    // Initialize used_list
    used_list->marked = 1;
    used_list->next = NULL;
    used_list->prev = NULL;
    used_list->size = 0;

    // Assign free_list and used_list to the allocator
    allocator->free_list = free_list;
    allocator->used_list = used_list;

    // Add remaining memory to the free list (if any)
    size_t used_memory = sizeof(allocator_t) + 2 * sizeof(mem_block_t);
    size_t remaining_memory = total_pages * PAGE_SIZE - used_memory;
    if (remaining_memory >= sizeof(mem_block_t)) {
        mem_block_t *remaining_block = (mem_block_t *)((char *)used_list + sizeof(mem_block_t));
        remaining_block->size = remaining_memory;
        add_to_free_list(allocator, remaining_block);
    }

    return allocator;
}

// Memory allocation
void *mem_alloc(allocator_t *allocator, size_t num_units) {
    if (allocator->allocated_size > ALLOC_LIMIT) {
        gc_collect(allocator);  // Garbage collect when allocation exceeds limit
    }

    size_t total_size = (num_units + sizeof(mem_block_t) - 1) / sizeof(mem_block_t) + 1;
    total_size *= sizeof(mem_block_t);

    mem_block_t *free_list = allocator->free_list;

    do {
        if (free_list->size >= total_size) {
            if (free_list->size == total_size) {
                free_list->prev->next = free_list->next;
                if (free_list->next != NULL) {
                    free_list->next->prev = free_list->prev;
                }

                free_list->next = allocator->used_list->next;
                if (allocator->used_list->next != NULL) {
                    allocator->used_list->next->prev = free_list;
                }
                free_list->prev = allocator->used_list;
                allocator->used_list->next = free_list;

                allocator->allocated_size += free_list->size;
                return (void *)(free_list + 1);
            }
            if (free_list->size - total_size >= sizeof(mem_block_t)) {
                mem_block_t *new_block = (mem_block_t *)((char *)free_list + total_size);
                new_block->size = free_list->size - total_size;
                new_block->next = free_list->next;
                new_block->prev = free_list->prev;
                free_list->prev->next = new_block;
                if (free_list->next != NULL) {
                    free_list->next->prev = new_block;
                }

                free_list->size = total_size;
                free_list->next = allocator->used_list->next;
                if (allocator->used_list->next != NULL) {
                    allocator->used_list->next->prev = free_list;
                }
                free_list->prev = allocator->used_list;
                allocator->used_list->next = free_list;

                allocator->allocated_size += free_list->size;
                return (void *)(free_list + 1);
            }
        }
        free_list = free_list->next;
    } while (free_list != allocator->free_list);

    size_t total_pages = (total_size + PAGE_SIZE - 1) / PAGE_SIZE;
    mem_block_t *new_block = mempage_fetch(total_pages);
    if (new_block == NULL) {
        return NULL;
    }

    new_block->next = NULL;
    new_block->prev = NULL;
    new_block->size = total_pages * PAGE_SIZE;

    add_to_free_list(allocator, new_block);
    return mem_alloc(allocator, num_units);
}

// Memory deallocation
u_int16_t mem_dealloc(allocator_t *allocator, void *mem) {
    mem_block_t *block = (mem_block_t *)mem - 1;

    if (block->prev != NULL) {
        block->prev->next = block->next;
    }
    if (block->next != NULL) {
        block->next->prev = block->prev;
    }

    if (add_to_free_list(allocator, block) == ALLOC_FAILURE) {
        return ALLOC_FAILURE;
    }

    return ALLOC_SUCCESS;
}

void print_freelist(mem_block_t *list) {
    printf("Free List: \n");
    mem_block_t *current = list->next;
    do {
        printf("Block at %p: size=%zu, marked=%d\n", (void *)current, current->size, current->marked);
        current = current->next;
    } while (current != NULL && current != list);
    printf("\n");
}

void print_usedlist(mem_block_t *list) {
    printf("Used List: \n");
    while (list != NULL) {
        printf("Block at %p: size=%zu, marked=%d\n", (void *)list, list->size, list->marked);
        list = list->next;
    }
    printf("\n");
}
