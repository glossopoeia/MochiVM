#ifndef zhenzhu_vm_h
#define zhenzhu_vm_h

#include "chunk.h"

typedef struct {
    Chunk * chunk;
} VM;

// Initialize an empty virtual machine state.
void initVM(VM * vm);
// Deallocate the given virtual machine state.
void freeVM(VM * vm);

#endif