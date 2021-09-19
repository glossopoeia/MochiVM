#ifndef zhenzhu_vm_h
#define zhenzhu_vm_h

#include "chunk.h"
#include "value.h"
#include "object.h"

#define STACK_MAX 256
#define CALL_STACK_MAX 512

typedef struct {
    Chunk * chunk;
    uint8_t * ip;

    // Value stack, which all instructions that consume and produce data operate upon.
    Value stack[STACK_MAX];
    Value * stackTop;

    // Frame stack, which variable, function, and continuation instructions operate upon.
    ObjVarFrame* callStack[CALL_STACK_MAX];
    ObjVarFrame** callStackTop;

    // A list of every object heap-allocated by the virtual machine.
    Obj* objects;
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

void pushFrame(ObjVarFrame* frame, VM* vm);
ObjVarFrame* popFrame(VM* vm);

ObjString* takeString(char* chars, int length, VM* vm);
ObjString* copyString(const char* chars, int length, VM* vm);

ObjVarFrame* newVarFrame(Value* vars, int varCount, VM* vm);
ObjCallFrame* newCallFrame(Value* vars, int varCount, uint8_t* afterLocation, VM* vm);

#endif