
#ifndef IJVM_STRUCT_H
#define IJVM_STRUCT_H

#include <stdio.h>  /* contains type FILE * */
#include <stdbool.h>
#include "ijvm_types.h"
/**
 * All the state of your IJVM machine goes in this struct!
 **/

typedef struct {
    word *elements;
    int top;
    int capacity;
} Stack;

typedef struct {
    word reference;  // Unique identifier for this heap object
    word* data;      // The actual array data
    int size;          // The number of elements in the array
} heap_object_t;

typedef struct IJVM {
    // do not changes these two variables
    FILE *in;   // use fgetc(ijvm->in) to get a character from in.
                // This will return EOF if no char is available.
    FILE *out;  // use for example fprintf(ijvm->out, "%c", value); to print value to out

  // your variables go here
    byte *text;
    word *constant_pool;
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
    word next_ref;       // Counter to generate unique array references

} ijvm;

#endif 
