#ifndef zhenzhu_vm_h
#define zhenzhu_vm_h

#include "chunk.h"
#include "value.h"
#include "object.h"

// The maximum number of temporary objects that can be made visible to the GC
// at one time.
#define ZHENZHU_MAX_TEMP_ROOTS 8

#define STACK_MAX 256
#define CALL_STACK_MAX 512

struct ZZVM {
    ZhenzhuConfiguration config;

    ByteBuffer* code;
    ValueBuffer* constants;
    IntBuffer* lines;
    uint8_t * ip;

    // Value stack, which all instructions that consume and produce data operate upon.
    Value stack[STACK_MAX];
    Value * stackTop;

    // Frame stack, which variable, function, and continuation instructions operate upon.
    ObjVarFrame* callStack[CALL_STACK_MAX];
    ObjVarFrame** callStackTop;

    // Memory management data:

    // The number of bytes that are known to be currently allocated. Includes all
    // memory that was proven live after the last GC, as well as any new bytes
    // that were allocated since then. Does *not* include bytes for objects that
    // were freed since the last GC.
    size_t bytesAllocated;

    // The number of total allocated bytes that will trigger the next GC.
    size_t nextGC;

    // The first object in the linked list of all currently allocated objects.
    Obj* objects;

    // The "gray" set for the garbage collector. This is the stack of unprocessed
    // objects while a garbage collection pass is in process.
    Obj** gray;
    int grayCount;
    int grayCapacity;

    // The list of temporary roots. This is for temporary or new objects that are
    // not otherwise reachable but should not be collected.
    //
    // They are organized as a stack of pointers stored in this array. This
    // implies that temporary roots need to have stack semantics: only the most
    // recently pushed object can be released.
    Obj* tempRoots[ZHENZHU_MAX_TEMP_ROOTS];

    int numTempRoots;
};

int addConstant(ZZVM* vm, Value value);
void writeChunk(ZZVM* vm, uint8_t instr, int line);

void push(Value value, ZZVM * vm);
Value pop(ZZVM * vm);

void pushFrame(ObjVarFrame* frame, ZZVM* vm);
ObjVarFrame* popFrame(ZZVM* vm);

ObjString* takeString(char* chars, int length, ZZVM* vm);
ObjString* copyString(const char* chars, int length, ZZVM* vm);

ObjVarFrame* newVarFrame(Value* vars, int varCount, ZZVM* vm);
ObjCallFrame* newCallFrame(Value* vars, int varCount, uint8_t* afterLocation, ZZVM* vm);

#endif