#ifndef mochivm_vm_h
#define mochivm_vm_h

#include "object.h"

typedef enum
{
#define OPCODE(name) CODE_##name,
#include "opcodes.h"
#undef OPCODE
} Code;

// The maximum number of temporary objects that can be made visible to the GC
// at one time.
#define MOCHIVM_MAX_TEMP_ROOTS 512

#define MOCHIVM_MAX_CALL_FRAME_SLOTS 65535
#define MOCHIVM_MAX_MARK_FRAME_SLOTS 256

DECLARE_BUFFER(ForeignFunction, MochiVMForeignMethodFn);

struct MochiVM {
    MochiVMConfiguration config;

    ObjCodeBlock *block;
    ObjFiber *fiber;

    Table heap;
    TableKey nextHeapKey;

    // Memory management data:

    // The number of bytes that are known to be currently allocated. Includes all
    // memory that was proven live after the last GC, as well as any new bytes
    // that were allocated since then. Does *not* include bytes for objects that
    // were freed since the last GC.
    size_t bytesAllocated;

    // The number of total allocated bytes that will trigger the next GC.
    size_t nextGC;

    // The first object in the linked list of all currently allocated objects.
    Obj *objects;

    // The "gray" set for the garbage collector. This is the stack of unprocessed
    // objects while a garbage collection pass is in process.
    Obj **gray;
    int grayCount;
    int grayCapacity;

    // The buffer of foreign function pointers the VM knows about.
    ForeignFunctionBuffer foreignFns;
};

bool mochiHasPermission(MochiVM *vm, int permissionId);
bool mochiRequestPermission(MochiVM *vm, int permissionId);
bool mochiRequestAllPermissions(MochiVM *vm, int permissionGroup);
void mochiRevokePermission(MochiVM *vm, int permissionId);

int addConstant(MochiVM *vm, Value value);
void writeChunk(MochiVM *vm, uint8_t instr, int line);
void writeLabel(MochiVM *vm, int byteIndex, int labelLength, const char *label);
char *getLabel(MochiVM *vm, int byteIndex);

// Mark [obj] as reachable and still in use. This should only be called
// during the sweep phase of a garbage collection.
void mochiGrayObj(MochiVM *vm, Obj *obj);

// Mark [value] as reachable and still in use. This should only be called
// during the sweep phase of a garbage collection.
void mochiGrayValue(MochiVM *vm, Value value);

// Mark the values in [buffer] as reachable and still in use. This should only
// be called during the sweep phase of a garbage collection.
void mochiGrayBuffer(MochiVM *vm, ValueBuffer *buffer);

// Processes every object in the gray stack until all reachable objects have
// been marked. After that, all objects are either white (freeable) or black
// (in use and fully traversed).
void mochiBlackenObjects(MochiVM *vm);

#endif