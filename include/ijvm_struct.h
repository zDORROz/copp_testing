#ifndef IJVM_STRUCT_H
#define IJVM_STRUCT_H

#include <stdio.h>
#include <stdbool.h>
#include "ijvm_types.h"

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
} ijvm;

#endif
