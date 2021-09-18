#ifndef zhenzhu_vm_h
#define zhenzhu_vm_h

#include "chunk.h"
#include "value.h"

#define STACK_MAX 256

typedef struct {
    Chunk * chunk;
    uint8_t * ip;
    Value stack[STACK_MAX];
    Value * stackTop;
} VM;

typedef enum {
    INTERPRET_OK,
    INTERPRET_RUNTIME_ERROR,
} InterpretResult;

// Initialize an empty virtual machine state.
void initVM(VM * vm);
// Deallocate the given virtual machine state.
void freeVM(VM * vm);
// Set the given vm state to run through the given chunk,
// starting with the first instruction in the chunk.
InterpretResult interpret(Chunk * chunk, VM * vm);
void push(Value value, VM * vm);
Value pop(VM * vm);

#endif