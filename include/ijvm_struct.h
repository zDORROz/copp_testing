#ifndef IJVM_STRUCT_H
#define IJVM_STRUCT_H

#include <stdio.h>
#include <stdbool.h>
#include "ijvm_types.h"

/**
 * @brief Represents a single allocated array on the heap.
 */
typedef struct {
    word_t reference;  // Unique identifier for this heap object
    word_t* data;      // The actual array data
    int size;          // The number of elements in the array
    bool marked;       // Flag for the garbage collector's mark phase
} heap_object_t;

typedef struct {
    word_t *elements;
    int top;
    int capacity;
} Stack;

typedef struct IJVM {
    FILE *in;
    FILE *out;

    byte_t *text;
    word_t *constant_pool;
    
    // Add constant_pool_size to the struct
    uint32_t constant_pool_size;
    uint32_t text_size;
    
    unsigned int program_counter;
    int lv_pointer;

    Stack *stack;
    bool halted;

    // --- Heap Management ---
    heap_object_t** heap; // A dynamic array of pointers to heap objects
    int heap_size;         // Current number of objects on the heap
    int heap_capacity;     // Current allocated capacity of the heap array
    word_t next_ref;       // Counter to generate unique array references

    // --- GC Tracking for Tests ---
    word_t* freed_refs;    // Stores references freed in the last GC cycle
    int freed_refs_size;   // Number of references in freed_refs
    int freed_refs_capacity; // Allocated capacity for freed_refs
} ijvm;

#endif
