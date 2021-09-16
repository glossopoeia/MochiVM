#ifndef zhenzhu_vm_h
#define zhenzhu_vm_h

#include "chunk.h"

typedef struct {
    Chunk * chunk;
    uint8_t * ip;
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

#endif