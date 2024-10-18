#include "gc.h"

// Enum to define garbage collection states
typedef enum {
    GC_STOPPED,
    GC_PAUSED,
    GC_RUNNING
} GCState;

// Static variable to hold the current state of the garbage collector
static GCState gc_state = GC_STOPPED;

// Function to initialize the garbage collector
void alloc_init() {
    // Initialization logic for your garbage collector
    gc_state = GC_RUNNING;  // Set initial state to RUNNING
}

// Function to allow the user to set the GC option at any time
void gc_set_option(GCState new_state) {
    gc_state = new_state;
    printf("Garbage Collector state set to: %s\n", 
           (gc_state == GC_RUNNING) ? "RUNNING" : 
           (gc_state == GC_PAUSED) ? "PAUSED" : "STOPPED");
}

// Function to start or resume garbage collection
void gc_start() {
    gc_set_option(GC_RUNNING);
}

// Function to stop garbage collection completely
void gc_stop() {
    gc_set_option(GC_STOPPED);
}

// Function to pause garbage collection temporarily
void gc_pause() {
    gc_set_option(GC_PAUSED);
}

// Scan memory to mark reachable blocks
static void scan_mem(alloc_t *allocator, uintptr_t *start, uintptr_t *end) {
    block_t *current_block = allocator->used_list->next;

    // Traverse through each pointer in the region defined by start and end
    for (; start < end; start++) {
        uintptr_t ptr_value = *start;

        // Check if the pointer value falls within any allocated block
        while (current_block != NULL) {
            if (current_block->marked) {
                current_block = current_block->next;
                continue;
            }

            uintptr_t block_start = (uintptr_t)current_block;
            uintptr_t block_end = block_start + 1 + current_block->size;

            if (ptr_value >= block_start && ptr_value < block_end) {
                current_block->marked = 1; // Mark the block as reachable
                break;
            }

            current_block = current_block->next;
        }

        // Reset to the start of the used list for the next pointer in the region
        current_block = allocator->used_list->next;
    }
}

// Core garbage collection function with state control (start, stop, pause)
void gc_collect(alloc_t *allocator) {
    // Check the current state of the garbage collection process
    if (gc_state == GC_PAUSED || gc_state == GC_STOPPED) {
        printf("Garbage Collection is currently %s\n", 
               (gc_state == GC_PAUSED) ? "PAUSED" : "STOPPED");
        return;  // Do not perform garbage collection if paused or stopped
    }

    printf("***Starting Garbage Collection Process*** Allocation size: %ld\n", allocator->allocated_size);

    // Get the base of stack and top of stack
    void *bos = allocator->bos;
    void *tos = __builtin_frame_address(0);

    // Scan the memory region
    scan_mem(allocator, tos, bos);

    // Mark phase: DFS on used blocks
    block_t *used_list = allocator->used_list->next;
    while (used_list != NULL) {
        if (used_list->marked == 0) {
            used_list = used_list->next;
            continue;
        }

        scan_mem(allocator, (void *)(used_list + 1), (void *)(used_list) + used_list->size);

        used_list = used_list->next;
    }

    // Sweep phase: Free unmarked (unreachable) blocks
    block_t *current = allocator->used_list->next;
    while (current != NULL) {
        if (current->marked) {
            current->marked = 0; // Reset mark for next collection
            current = current->next;
        } else {
            // Block is not marked (unreachable), free it
            block_t *unused_block = current;
            current = current->next;

            // Remove from used list
            if (unused_block->prev != NULL) {
                unused_block->prev->next = unused_block->next;
            }
            if (unused_block->next != NULL) {
                unused_block->next->prev = unused_block->prev;
            }

            // Add to free list
            size_t size = unused_block->size;
            if (add_to_free_list(allocator, unused_block) != ALLOC_FAILURE) {
                allocator->allocated_size -= size;
            }
        }
    }
}
