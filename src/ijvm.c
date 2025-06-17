#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "ijvm.h"
#include "util.h"

// --- Stack Utilities ---
Stack* create_stack(int capacity) {
    Stack* s = (Stack*) malloc(sizeof(Stack));
    s->capacity = capacity;
    s->top = -1;
    s->elements = (word_t*) malloc(s->capacity * sizeof(word_t));
    return s;
}

void destroy_stack(Stack* s) {
    if (s) {
        free(s->elements);
        free(s);
    }
}

void push(Stack* s, word_t value) {
    if (s->top >= s->capacity - 1) {
        s->capacity *= 2;
        s->elements = (word_t*) realloc(s->elements, s->capacity * sizeof(word_t));
    }
    s->top++;
    s->elements[s->top] = value;
}

word_t pop(Stack* s) {
    if (s->top < 0) return 0;
    return s->elements[s->top--];
}

// --- Method Invocation Logic ---
void invoke_method(ijvm* m, uint16_t method_index) {
    // Use the struct member for the check
    if (method_index >= (m->constant_pool_size / 4)) { m->halted = true; return; }
    uint32_t method_address = get_constant(m, method_index);
    if (method_address + 3 >= m->text_size) { m->halted = true; return; }

    uint16_t num_params = read_uint16(&m->text[method_address]);
    uint16_t num_locals = read_uint16(&m->text[method_address + 2]);

    if (m->stack->top < (int)num_params - 1) { m->halted = true; return; }

    int new_lv = m->stack->top - (num_params - 1);
    int link_ptr_target = new_lv + num_params + num_locals;

    for (int i=0; i < num_locals; ++i) push(m->stack, 0);

    push(m->stack, m->program_counter);
    push(m->stack, m->lv_pointer);

    m->stack->elements[new_lv] = link_ptr_target;
    m->lv_pointer = new_lv;
    m->program_counter = method_address + 4;
}

void return_from_method(ijvm* m) {
    if (m->stack->top < 0) { m->halted = true; return; }
    word_t return_value = pop(m->stack);

    if (m->lv_pointer == 0) { m->halted = true; return; }

    int link_ptr_target = m->stack->elements[m->lv_pointer];

    int restored_pc = m->stack->elements[link_ptr_target];
    int restored_lv = m->stack->elements[link_ptr_target + 1];

    m->stack->top = m->lv_pointer - 1;

    m->program_counter = restored_pc;
    m->lv_pointer = restored_lv;

    push(m->stack, return_value);
}

// --- Main IJVM Logic ---
ijvm* init_ijvm(char *binary_path, FILE* input , FILE* output)
{
    ijvm* m = (ijvm*) malloc(sizeof(ijvm));
    m->in = input; m->out = output; m->halted = false;

    FILE *binary = fopen(binary_path, "rb");
    if (!binary) { free(m); return NULL; }

    fseek(binary, 0, SEEK_END);
    long file_size = ftell(binary);
    fseek(binary, 0, SEEK_SET);

    byte_t magic_buf[4];
    if (fread(magic_buf, 4, 1, binary) != 1 || read_uint32(magic_buf) != 0x1DEADFAD) {
        fclose(binary); free(m); return NULL;
    }

    fseek(binary, 8, SEEK_SET);
    byte_t constants_size_buf[4];
    if(fread(constants_size_buf, 4, 1, binary) != 1) { fclose(binary); free(m); return NULL; }
    
    // Store the constant pool size in the struct
    m->constant_pool_size = read_uint32(constants_size_buf);

    if ((ftell(binary) + m->constant_pool_size) > (unsigned long)file_size) { fclose(binary); free(m); return NULL; }

    m->constant_pool = (word_t*) malloc(m->constant_pool_size == 0 ? 4 : m->constant_pool_size);
    if (!m->constant_pool) { fclose(binary); free(m); return NULL; }
    
    if (m->constant_pool_size > 0) {
        if (fread(m->constant_pool, m->constant_pool_size, 1, binary) != 1) {
            free(m->constant_pool); fclose(binary); free(m); return NULL;
        }
    }
    for (uint32_t i = 0; i < m->constant_pool_size / 4; i++) {
        m->constant_pool[i] = swap_uint32(m->constant_pool[i]);
    }

    fseek(binary, 16 + m->constant_pool_size, SEEK_SET);
    byte_t text_size_buf[4];
    if(fread(text_size_buf, 4, 1, binary) != 1) { free(m->constant_pool); fclose(binary); free(m); return NULL; }
    m->text_size = read_uint32(text_size_buf);
    if ((ftell(binary) + m->text_size) > (unsigned long)file_size) {
        free(m->constant_pool); fclose(binary); free(m); return NULL;
    }

    m->text = (byte_t*) malloc(m->text_size);
    if (!m->text) { free(m->constant_pool); fclose(binary); free(m); return NULL; }
    
    if (m->text_size > 0) {
        if (fread(m->text, m->text_size, 1, binary) != 1) {
            free(m->constant_pool); free(m->text); fclose(binary); free(m); return NULL;
        }
    }
    fclose(binary);

    m->stack = create_stack(65536);
    m->program_counter = 0;
    m->lv_pointer = 0;
    for (int i = 0; i < 1024; ++i) push(m->stack, 0);

    return m;
}

void step(ijvm* m)
{
    if (finished(m)) return;

    byte_t instruction = m->text[m->program_counter++];
    
    switch (instruction) {
        case OP_LDC_W: {
            if (m->program_counter + 1 >= m->text_size) { m->halted = true; break; }
            uint16_t const_index = read_uint16(&m->text[m->program_counter]);
            // Use the stored size for the check
            if (const_index >= (m->constant_pool_size / 4)) { m->halted = true; break; }
            m->program_counter += 2;
            push(m->stack, get_constant(m, const_index));
            break;
        }
        case OP_BIPUSH:
            if (m->program_counter >= m->text_size) { m->halted = true; break; }
            push(m->stack, (int8_t)m->text[m->program_counter++]);
            break;
        case OP_DUP:
            if (m->stack->top < 0) { m->halted = true; break; }
            push(m->stack, tos(m));
            break;
        case OP_GOTO: {
            if (m->program_counter + 1 >= m->text_size) { m->halted = true; break; }
            int16_t offset = read_int16(&m->text[m->program_counter]);
            int target_pc = (m->program_counter - 1) + offset;
            if (target_pc < 0 || (unsigned int)target_pc >= m->text_size) { m->halted = true; break; }
            m->program_counter = target_pc;
            break;
        }
        case OP_IADD: case OP_IAND: case OP_IOR: case OP_ISUB: {
            if (m->stack->top < 1) { m->halted = true; break; }
            word_t val2 = pop(m->stack);
            word_t val1 = pop(m->stack);
            if (instruction == OP_IADD) push(m->stack, val1 + val2);
            else if (instruction == OP_ISUB) push(m->stack, val1 - val2);
            else if (instruction == OP_IAND) push(m->stack, val1 & val2);
            else if (instruction == OP_IOR) push(m->stack, val1 | val2);
            break;
        }
        case OP_IFEQ: case OP_IFLT: {
            if (m->stack->top < 0) { m->halted = true; break; }
            word_t val = pop(m->stack);
            if (m->program_counter + 1 >= m->text_size) { m->halted = true; break; }
            int16_t offset = read_int16(&m->text[m->program_counter]);
            if ((instruction == OP_IFEQ && val == 0) || (instruction == OP_IFLT && val < 0)) {
                int target_pc = (m->program_counter - 1) + offset;
                if (target_pc < 0 || (unsigned int)target_pc >= m->text_size) { m->halted = true; break; }
                m->program_counter = target_pc;
            } else {
                m->program_counter += 2;
            }
            break;
        }
        case OP_IF_ICMPEQ: {
            if (m->stack->top < 1) { m->halted = true; break; }
            word_t val2 = pop(m->stack);
            word_t val1 = pop(m->stack);
            if (m->program_counter + 1 >= m->text_size) { m->halted = true; break; }
            int16_t offset = read_int16(&m->text[m->program_counter]);
            if (val1 == val2) {
                int target_pc = (m->program_counter - 1) + offset;
                if (target_pc < 0 || (unsigned int)target_pc >= m->text_size) { m->halted = true; break; }
                m->program_counter = target_pc;
            } else {
                m->program_counter += 2;
            }
            break;
        }
        case OP_IINC: {
            if (m->program_counter + 1 >= m->text_size) { m->halted = true; break; }
            uint8_t var = m->text[m->program_counter++];
            int8_t val = m->text[m->program_counter++];
            m->stack->elements[m->lv_pointer + var] += val;
            break;
        }
        case OP_ILOAD: {
            if (m->program_counter >= m->text_size) { m->halted = true; break; }
            uint8_t var = m->text[m->program_counter++];
            push(m->stack, get_local_variable(m, var));
            break;
        }
        case OP_INVOKEVIRTUAL: {
            if (m->program_counter + 1 >= m->text_size) { m->halted = true; break; }
            uint16_t method_index = read_uint16(&m->text[m->program_counter]);
            m->program_counter += 2;
            invoke_method(m, method_index);
            break;
        }
        case OP_IRETURN:
            return_from_method(m);
            break;
        case OP_ISTORE: {
            if (m->stack->top < 0) { m->halted = true; break; }
            if (m->program_counter >= m->text_size) { m->halted = true; break; }
            uint8_t var = m->text[m->program_counter++];
            m->stack->elements[m->lv_pointer + var] = pop(m->stack);
            break;
        }
        case OP_NOP: break;
        case OP_POP:
            if (m->stack->top < 0) { m->halted = true; break; }
            pop(m->stack);
            break;
        case OP_SWAP: {
            if (m->stack->top < 1) { m->halted = true; break; }
            word_t val1 = pop(m->stack);
            word_t val2 = pop(m->stack);
            push(m->stack, val1);
            push(m->stack, val2);
            break;
        }
        case OP_WIDE: {
            if (m->program_counter >= m->text_size) { m->halted = true; break; }
            byte_t wide_op = m->text[m->program_counter++];
            if (m->program_counter + 1 >= m->text_size) { m->halted = true; break; }
            uint16_t index = read_uint16(&m->text[m->program_counter]);
            m->program_counter += 2;

            if (wide_op == OP_ILOAD) {
                push(m->stack, get_local_variable(m, index));
            } else if (wide_op == OP_ISTORE) {
                if (m->stack->top < 0) { m->halted = true; break; }
                m->stack->elements[m->lv_pointer + index] = pop(m->stack);
            } else if (wide_op == OP_IINC) {
                if (m->program_counter >= m->text_size) { m->halted = true; break; }
                int8_t val = m->text[m->program_counter++];
                m->stack->elements[m->lv_pointer + index] += val;
            } else { m->halted = true; }
            break;
        }
        case OP_HALT: m->halted = true; break;
        case OP_ERR:
            fprintf(m->out, "ERROR: An error occurred.\n");
            m->halted = true;
            break;
        case OP_OUT:
            if (m->stack->top < 0) { m->halted = true; break; }
            fprintf(m->out, "%c", (char)pop(m->stack));
            break;
        case OP_IN: {
            int c = fgetc(m->in);
            push(m->stack, (c == EOF) ? 0 : (word_t)c);
            break;
        }
        default: m->halted = true; break;
    }
}


// --- Utility and Other Functions ---
void destroy_ijvm(ijvm* m) { if(m){destroy_stack(m->stack);free(m->text);free(m->constant_pool);free(m);} }
byte_t *get_text(ijvm* m) { return m->text; }
unsigned int get_text_size(ijvm* m) { return m->text_size; }
word_t get_constant(ijvm* m, int i) { return m->constant_pool[i]; }
unsigned int get_program_counter(ijvm* m) { return m->program_counter; }
word_t tos(ijvm* m) { return m->stack->top < 0 ? 0 : m->stack->elements[m->stack->top]; }
bool finished(ijvm* m) { return m->halted || m->program_counter >= m->text_size; }
word_t get_local_variable(ijvm* m, int i) { return m->stack->elements[m->lv_pointer + i]; }
byte_t get_instruction(ijvm* m) { return get_text(m)[get_program_counter(m)]; }
ijvm* init_ijvm_std(char *binary_path) { return init_ijvm(binary_path, stdin, stdout); }
void run(ijvm* m) { while (!finished(m)) step(m); }
int get_call_stack_size(ijvm* m) { return 0; }
bool is_heap_freed(ijvm* m, word_t reference) { return false; }
