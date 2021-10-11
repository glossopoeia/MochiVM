#ifndef zhenzhu_vm_h
#define zhenzhu_vm_h

#include "value.h"

typedef enum
{
    #define OPCODE(name) CODE_##name,
    #include "opcodes.h"
    #undef OPCODE
} Code;

// The maximum number of temporary objects that can be made visible to the GC
// at one time.
#define ZHENZHU_MAX_TEMP_ROOTS 8

#define STACK_MAX 256
#define CALL_STACK_MAX 512

struct ZZVM {
    ZhenzhuConfiguration config;

    ObjCodeBlock* block;
    uint8_t* ip;
    ObjFiber* fiber;

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

void pushFrame(ObjVarFrame* frame, ZZVM* vm);
ObjVarFrame* popFrame(ZZVM* vm);

// Marks [obj] as a GC root so that it doesn't get collected.
void zzPushRoot(ZZVM* vm, Obj* obj);

// Removes the most recently pushed temporary root.
void zzPopRoot(ZZVM* vm);

ObjString* takeString(char* chars, int length, ZZVM* vm);
ObjString* copyString(const char* chars, int length, ZZVM* vm);

ObjVarFrame* newVarFrame(Value* vars, int varCount, ZZVM* vm);
ObjCallFrame* newCallFrame(Value* vars, int varCount, uint8_t* afterLocation, ZZVM* vm);

// Mark [obj] as reachable and still in use. This should only be called
// during the sweep phase of a garbage collection.
void zzGrayObj(ZZVM* vm, Obj* obj);

// Mark [value] as reachable and still in use. This should only be called
// during the sweep phase of a garbage collection.
void zzGrayValue(ZZVM* vm, Value value);

// Mark the values in [buffer] as reachable and still in use. This should only
// be called during the sweep phase of a garbage collection.
void zzGrayBuffer(ZZVM* vm, ValueBuffer* buffer);

// Processes every object in the gray stack until all reachable objects have
// been marked. After that, all objects are either white (freeable) or black
// (in use and fully traversed).
void zzBlackenObjects(ZZVM* vm);

#endif