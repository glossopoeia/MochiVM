#ifndef mochivm_vm_h
#define mochivm_vm_h

#include "value.h"

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

typedef struct
{
    // The entry's key. 0 if not in use & available, 1 if tombstone, >1 for actual value.
    // A tombstone is an entry that was previously in use but is now deleted.
    HeapKey key;

    // The value associated with the key.
    Value value;
} HeapEntry;

// A hash table mapping keys to values.
//
// We use something very simple: open addressing with linear probing. The hash
// table is an array of entries. Each entry is a key-value pair. If the key is
// either 0 or 1, it indicates no value is currently in that slot.
// Otherwise, it's a valid key, and the value is the value associated with it.
//
// When entries are added, the array is dynamically scaled by GROW_FACTOR to
// keep the number of filled slots under MAP_LOAD_PERCENT. Likewise, if the map
// gets empty enough, it will be resized to a smaller array. When this happens,
// all existing entries are rehashed and re-added to the new array.
//
// When an entry is removed, its slot is replaced with a "tombstone". This is an
// entry whose key is 1. When probing
// for a key, we will continue past tombstones, because the desired key may be
// found after them if the key that was removed was part of a prior collision.
// When the array gets resized, all tombstones are discarded.
typedef struct
{
    // The number of entries allocated.
    uint32_t capacity;

    // The number of entries in the map.
    uint32_t count;

    // Pointer to a contiguous array of [capacity] entries.
    HeapEntry* entries;
} Heap;

struct MochiVM {
    MochiVMConfiguration config;

    ObjCodeBlock* block;
    ObjFiber* fiber;

    Heap heap;
    uint64_t nextHeapKey;

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

    // The buffer of foreign function pointers the VM knows about.
    ForeignFunctionBuffer foreignFns;
};

// Creates a new empty heap.
Heap* mochiNewHeap(MochiVM* vm);
// Looks up [key] in [heap]. If found, returns true and sets the out Value from the entry.
// Otherwise, sets `FALSE_VAL` as the out Value and returns false.
bool mochiHeapGet(Heap* heap, HeapKey key, Value* out);
// Associates [key] with [value] in [heap].
void mochiHeapSet(MochiVM* vm, Heap* heap, HeapKey key, Value value);
void mochiHeapClear(MochiVM* vm, Heap* heap);
// Removes [key] from [heap], if present. Returns true if found or false otherwise.
bool mochiHeapTryRemove(MochiVM* vm, Heap* heap, HeapKey key);

int addConstant(MochiVM* vm, Value value);
void writeChunk(MochiVM* vm, uint8_t instr, int line);
void writeLabel(MochiVM* vm, int byteIndex, int labelLength, const char* label);
char* getLabel(MochiVM* vm, int byteIndex);

// Mark [obj] as reachable and still in use. This should only be called
// during the sweep phase of a garbage collection.
void mochiGrayObj(MochiVM* vm, Obj* obj);

// Mark [value] as reachable and still in use. This should only be called
// during the sweep phase of a garbage collection.
void mochiGrayValue(MochiVM* vm, Value value);

// Mark the values in [buffer] as reachable and still in use. This should only
// be called during the sweep phase of a garbage collection.
void mochiGrayBuffer(MochiVM* vm, ValueBuffer* buffer);

// Processes every object in the gray stack until all reachable objects have
// been marked. After that, all objects are either white (freeable) or black
// (in use and fully traversed).
void mochiBlackenObjects(MochiVM* vm);

#endif