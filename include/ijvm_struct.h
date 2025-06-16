
#ifndef IJVM_STRUCT_H
#define IJVM_STRUCT_H

#include <stdio.h>  /* contains type FILE * */

#include "ijvm_types.h"
/**
 * All the state of your IJVM machine goes in this struct!
 **/

typedef struct {
    int *elements;
    int top;
    int capacity;
} Stack;

typedef struct IJVM {
    // do not changes these two variables
    FILE *in;   // use fgetc(ijvm->in) to get a character from in.
                // This will return EOF if no char is available.
    FILE *out;  // use for example fprintf(ijvm->out, "%c", value); to print value to out

  // your variables go here
    #define EXPECTED_MAGIC_NUMBER 0x1DEADFAD
    uint32_t magic_number;
    uint32_t constant_pool_size;
    uint32_t text_size;
    uint32_t* constant_pool;
    uint8_t* text;
    unsigned int program_counter;
    Stack* stack;
    bool halted;
    int16_t offset;
    int local_variable_count;
    int lv_pointer;
    int stack_pointer;
    uint16_t method_index;
} ijvm;

#endif 
