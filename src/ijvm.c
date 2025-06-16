#include <stdio.h>  // for getc, printf
#include <stdlib.h> // malloc, free
#include "ijvm.h"
#include "util.h" // read this file for debug prints, endianness helper functions
#include "assert.h"


// see ijvm.h for descriptions of the below functions 


uint32_t get_int(FILE* file) {
  uint8_t bytes[4];
  fread(bytes, 1, 4, file);
  uint32_t integer = read_uint32(bytes);
  return integer;
}

void push(ijvm* m, int i) {
  if (m->stack->top == m->stack->capacity - 1) {
    m->stack->capacity *= 2;
    m->stack->elements = (int*)realloc(m->stack->elements, m->stack->capacity * 4);
    }
    m->stack->top += 1;
    m->stack->elements[m->stack->top] = i;
    return;
}

ijvm* init_ijvm(char *binary_path, FILE* input , FILE* output) 
{
  // do not change these first three  lines
  ijvm* m = (ijvm*) malloc(sizeof(ijvm));
  // note that malloc gives you memory, but gives no guarantees on the initial
  // values of that memory. It might be all zeroes, or be random data.
  // It is hence important that you initialize all variables in the ijvm
  // struct and do not assume these are set to zero.
  m->in = input;
  m->out = output;
  
  FILE *file = fopen(binary_path, "rb");

  m->magic_number = get_int(file);
  if (m->magic_number != EXPECTED_MAGIC_NUMBER) {
        fclose(file);
        free(m);
        return NULL;
    }

  fseek(file, 4, SEEK_CUR);

  m->constant_pool_size = get_int(file);

  m->constant_pool = (uint32_t*) malloc(m->constant_pool_size);
  for (uint32_t i = 0; i < (m->constant_pool_size / 4); i++) {
    m->constant_pool[i] = get_int(file);
  }

  fseek(file, 4, SEEK_CUR);

  m->text_size = get_int(file);

  m->text = (uint8_t*) malloc(m->text_size);
  fread(m->text, 1, m->text_size, file);

  fclose(file);

  m->stack = (Stack*)malloc(sizeof(Stack));
  m->stack->capacity = 512;
  m->stack->top = -1;
  m->stack->elements = (int*)malloc(m->stack->capacity * 4);
  for(int i = 0; i < 256; i++) {
    push(m, 0x00);
  }
  m->program_counter = 0;
  m->halted = false;
  m->offset = 256;
  m->lv_pointer = 0;
  m->stack_pointer = 256;

  return m;
}

void invoke_method(ijvm* m, uint16_t method_index) {
  uint32_t method_address = m->constant_pool[method_index];
  uint16_t num_args = swap_int16(m->text[method_address]) | (uint16_t) m->text[method_address + 1];
  uint16_t num_locals = swap_int16(m->text[method_address + 2]) | (uint16_t) m->text[method_address + 3];
  uint8_t method_start = method_address + 4;

  int new_lv = m->stack->top - num_args + 1;

  for(int i = 0; i < num_locals; i++) {
    push(m, 0x00);
  }

  push(m, m->stack_pointer);
  push(m, m->program_counter);
  push(m, m->lv_pointer);

  m->lv_pointer = new_lv;
  m->program_counter = method_start;
  m->stack_pointer = m->stack->top;

}

void return_from_method(ijvm* m)  {
  int new_stack_pointer = m->lv_pointer - 1;

  int return_value = m->stack->elements[m->stack->top];
  m->lv_pointer = m->stack->elements[m->stack_pointer];
  m->program_counter = m->stack->elements[m->stack_pointer - 1];
  m->stack_pointer = m->stack->elements[m->stack_pointer - 2];
  m->stack->top = new_stack_pointer;

  push(m, return_value);
}

void destroy_ijvm(ijvm* m) 
{
  free(m->constant_pool);
  free(m->text);
  free(m->stack->elements);
  free(m->stack);
  free(m);
}

byte_t *get_text(ijvm* m) 
{
  return m->text;
}

unsigned int get_text_size(ijvm* m) 
{
  return m->text_size; 
}

word_t get_constant(ijvm* m,int i) 
{
  return m->constant_pool[i];
}

unsigned int get_program_counter(ijvm* m) 
{
  return m->program_counter;
}

word_t tos(ijvm* m) 
{

  return m->stack->elements[m->stack->top];
}

bool finished(ijvm* m) 
{
  return m->halted || m->program_counter >= m->text_size;
}

word_t get_local_variable(ijvm* m, int i) 
{
  return m->stack->elements[m->lv_pointer + i];
}

void step(ijvm* m) 
{
 
  if (finished(m)) {
        return;
  }
    int instruction = m->text[m->program_counter];
    m->program_counter++;

    switch (instruction) {
      case OP_BIPUSH: {
        int value = (int8_t)m->text[m->program_counter++];
        push(m, value);
        break;
      }
      case OP_DUP:{
        assert(m->stack->top >= 0);
        push(m, m->stack->elements[m->stack->top]);
        break;
      }
      case OP_IADD:{
        assert(m->stack->top >= 1);
        m->stack->elements[m->stack->top - 1] = m->stack->elements[m->stack->top - 1] + m->stack->elements[m->stack->top];
        m->stack->top--;
        break;
      }
      case OP_IAND:{
        assert(m->stack->top >= 1);
        m->stack->elements[m->stack->top - 1] = m->stack->elements[m->stack->top - 1] & m->stack->elements[m->stack->top];
        m->stack->top--;
        break;
      }
      case OP_IOR:{
        assert(m->stack->top >= 1);
        m->stack->elements[m->stack->top - 1] = m->stack->elements[m->stack->top - 1] | m->stack->elements[m->stack->top];
        m->stack->top--;
        break;
      }
      case OP_ISUB:{
        assert(m->stack->top >= 1);
        m->stack->elements[m->stack->top - 1] = m->stack->elements[m->stack->top - 1] - m->stack->elements[m->stack->top];
        m->stack->top--;
        break;
      }
      case OP_NOP:{
        break;
      }
      case OP_POP:{
        assert(m->stack->top >= 0);
        m->stack->top -= 1;
        break;
      }
      case OP_SWAP:{
        assert(m->stack->top >= 1);
        int value = m->stack->elements[m->stack->top];
        m->stack->elements[m->stack->top] = m->stack->elements[m->stack->top - 1];
        m->stack->elements[m->stack->top - 1] = value;
        break;
      }
      case OP_ERR:{
        m->halted = true;
        fprintf(m->out, "ERROR, halting the emulator.\n");
        break;
      }
      case OP_HALT:{
        m->halted = true;
        break;
      }
      case OP_IN: {
        char c = fgetc(m->in);
        if(c == EOF) {
          c = 0;
        }
        push(m, c);
        break;
      }
      case OP_OUT:{
        fprintf(m->out, "%c", (char) m->stack->elements[m->stack->top]);
        m->stack->top -= 1;
        break;
      }
      case OP_GOTO: {
        int goto_offset = swap_int16(m->text[m->program_counter]) | (int) m->text[m->program_counter + 1];
        m->program_counter += goto_offset - 1;
        break;
      }
      case OP_IFEQ: {
        int value = m->stack->elements[m->stack->top--];
        if (value == 0) {
          int goto_offset = swap_int16(m->text[m->program_counter]) | (int) m->text[m->program_counter + 1];
          m->program_counter += goto_offset - 1;
        }
        else {
          m->program_counter += 2;
        }
        break;
      }
      case OP_IFLT: {
        int value = m->stack->elements[m->stack->top--];
        if (value < 0) {
          int goto_offset = swap_int16(m->text[m->program_counter]) | (uint16_t) m->text[m->program_counter + 1];
          m->program_counter += goto_offset - 1;
        }
        else {
          m->program_counter += 2;
        }
        break;
      }
      case OP_IF_ICMPEQ:{
        if (m->stack->elements[m->stack->top] == m->stack->elements[m->stack->top - 1]) {
          int goto_offset = swap_int16(m->text[m->program_counter]) | (uint16_t) m->text[m->program_counter + 1];
          m->program_counter += goto_offset - 1;
        }
        else {
          m->program_counter += 2;
        }
        m->stack->top -= 2;
        break;
      }
      case OP_LDC_W: {
        int constant_index = swap_int16(m->text[m->program_counter]) | (uint16_t) m->text[m->program_counter + 1];
        m->program_counter += 2;
        int32_t constant = get_constant(m, constant_index);
        push(m, constant);
        break;
      }
      case OP_ISTORE: {
        int lv_index = m->text[m->program_counter++];
        assert(m->stack->top >= 0);
        m->stack->elements[m->lv_pointer + lv_index] = m->stack->elements[m->stack->top--];
        break;
      }
      case OP_ILOAD: {
        int lv_index = m->text[m->program_counter++];
        int lv = get_local_variable(m, lv_index);
        push(m, lv);
        break;
      }
      case OP_IINC: {
        int lv_index = m->text[m->program_counter++];
        int value = (int8_t)m->text[m->program_counter++];
        m->stack->elements[m->lv_pointer + lv_index] += value;
        break;
      }
      case OP_WIDE:{
        int wide_instruction = m->text[m->program_counter++];
        uint16_t index = swap_int16(m->text[m->program_counter]) | (uint16_t) m->text[m->program_counter + 1];
        m->program_counter += 2;
        switch (wide_instruction) {
          case OP_ISTORE:{
            assert(m->stack->top >= 0);
            m->stack->elements[m->lv_pointer + index] = m->stack->elements[m->stack->top--];
            break;
          }
          case OP_ILOAD: {
            int lv = get_local_variable(m, index);
            push(m, lv);
            break;
          }
          case OP_IINC:{
            int value = (int8_t)m->text[m->program_counter++];
            m->stack->elements[m->lv_pointer + index] += value;
            break;
          }
        }
      break;
      }
      case OP_INVOKEVIRTUAL:{
        m->method_index = swap_int16(m->text[m->program_counter]) | (uint16_t) m->text[m->program_counter + 1];
        m->program_counter += 2;
        invoke_method(m, m->method_index);
        break;
      }
      case OP_IRETURN:{
        return_from_method(m);
        break;
      }
    }
    
}

byte_t get_instruction(ijvm* m) 
{ 
  return get_text(m)[get_program_counter(m)]; 
}

ijvm* init_ijvm_std(char *binary_path) 
{
  return init_ijvm(binary_path, stdin, stdout);
}

void run(ijvm* m) 
{
  while (!finished(m)) 
  {
    step(m);
  }
}


// Below: methods needed by bonus assignments, see ijvm.h
// You can leave these unimplemented if you are not doing these bonus 
// assignments.

int get_call_stack_size(ijvm* m) 
{
   // TODO: implement me if doing tail call bonus
   return 0;
}


// Checks if reference is a freed heap array. Note that this assumes that 
// 
bool is_heap_freed(ijvm* m, word_t reference) 
{
   // TODO: implement me if doing garbage collection bonus
   return 0;
}
