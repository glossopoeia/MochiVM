#include <stdio.h>
#include <string.h>

#include "common.h"
#include "debug.h"
#include "memory.h"
#include "vm.h"

#if MOCHIVM_DEBUG_TRACE_MEMORY || MOCHIVM_DEBUG_TRACE_GC
    #include <time.h>
#endif

#if MOCHIVM_BATTERY_UV
    #include "uv.h"
    #include "battery_uv.h"
#endif

// TODO: Tune these.
// The initial (and minimum) capacity of a non-empty list or map object.
#define HEAP_MIN_CAPACITY 16

// The rate at which a collection's capacity grows when the size exceeds the
// current capacity. The new capacity will be determined by *multiplying* the
// old capacity by this. Growing geometrically is necessary to ensure that
// adding to a collection has O(1) amortized complexity.
#define HEAP_GROW_FACTOR 2

// The maximum percentage of map entries that can be filled before the map is
// grown. A lower load takes more memory but reduces collisions which makes
// lookup faster.
#define HEAP_LOAD_PERCENT 75

DEFINE_BUFFER(ForeignFunction, MochiVMForeignMethodFn);

// The behavior of realloc() when the size is 0 is implementation defined. It
// may return a non-NULL pointer which must not be dereferenced but nevertheless
// should be freed. To prevent that, we avoid calling realloc() with a zero
// size.
static void* defaultReallocate(void* ptr, size_t newSize, void* _) {
    if (newSize == 0) {
        free(ptr);
        return NULL;
    }
    return realloc(ptr, newSize);
}

#if MOCHIVM_BATTERY_UV
static void* uvmochiMalloc(size_t size) { return defaultReallocate(NULL, size, NULL); }
static void* uvmochiRealloc(void* ptr, size_t size) { return defaultReallocate(ptr, size, NULL); }
static void* uvmochiCalloc(size_t count, size_t size) {
    void* mem = defaultReallocate(NULL, count * size, NULL);
    memset(mem, 0, count * size);
    return mem;
}
static void uvmochiFree(void* ptr) { defaultReallocate(NULL, 0, NULL); }
#endif

int mochiGetVersionNumber() { 
    return MOCHIVM_VERSION_NUMBER;
}

void mochiInitConfiguration(MochiVMConfiguration* config) {
    config->reallocateFn = defaultReallocate;
    config->errorFn = NULL;
    config->valueStackCapacity = 128;
    config->frameStackCapacity = 512;
    config->rootStackCapacity = 16;
    config->initialHeapSize = 1024 * 1024 * 10;
    config->minHeapSize = 1024 * 1024;
    config->heapGrowthPercent = 50;
    config->userData = NULL;
}

MochiVM* mochiNewVM(MochiVMConfiguration* config) {
    MochiVMReallocateFn reallocate = defaultReallocate;
    void* userData = NULL;
    if (config != NULL) {
        userData = config->userData;
        reallocate = config->reallocateFn ? config->reallocateFn : defaultReallocate;
    }
    
    MochiVM* vm = (MochiVM*)reallocate(NULL, sizeof(*vm), userData);
    memset(vm, 0, sizeof(MochiVM));

    // Copy the configuration if given one.
    if (config != NULL) {
        memcpy(&vm->config, config, sizeof(MochiVMConfiguration));

        // We choose to set this after copying, 
        // rather than modifying the user config pointer
        vm->config.reallocateFn = reallocate;
    } else {
        mochiInitConfiguration(&vm->config);
    }

    // TODO: Should we allocate and free this during a GC?
    vm->grayCount = 0;
    // TODO: Tune this.
    vm->grayCapacity = 4;
    vm->gray = (Obj**)reallocate(NULL, vm->grayCapacity * sizeof(Obj*), userData);
    vm->nextGC = vm->config.initialHeapSize;

    vm->block = mochiNewCodeBlock(vm);
    mochiForeignFunctionBufferInit(&vm->foreignFns);
    vm->heap.count = 0;
    vm->heap.capacity = 0;
    vm->heap.entries = NULL;
    // start at 2 since 0 and 1 are reserved for available/tombstoned slots
    vm->nextHeapKey = 2;

#if MOCHIVM_BATTERY_UV
    uv_replace_allocator(uvmochiMalloc, uvmochiRealloc, uvmochiCalloc, uvmochiFree);

    mochiAddForeign(vm, uvmochiNewTimer);
    mochiAddForeign(vm, uvmochiCloseTimer);
    mochiAddForeign(vm, uvmochiTimerStart);
#endif

    return vm;
}

void mochiFreeVM(MochiVM* vm) {

    // Free all of the GC objects.
    Obj* obj = vm->objects;
    while (obj != NULL) {
        Obj* next = obj->next;
        mochiFreeObj(vm, obj);
        obj = next;
    }

    // Free up the GC gray set.
    vm->gray = (Obj**)vm->config.reallocateFn(vm->gray, 0, vm->config.userData);

    mochiForeignFunctionBufferClear(vm, &vm->foreignFns);
    mochiHeapClear(vm, &vm->heap);
    DEALLOCATE(vm, vm);
}

void mochiCollectGarbage(MochiVM* vm) {
#if MOCHIVM_DEBUG_TRACE_MEMORY || MOCHIVM_DEBUG_TRACE_GC
    printf("-- gc --\n");

    size_t before = vm->bytesAllocated;
    double startTime = (double)clock() / CLOCKS_PER_SEC;
#endif

    // Mark all reachable objects.

    // Reset this. As we mark objects, their size will be counted again so that
    // we can track how much memory is in use without needing to know the size
    // of each *freed* object.
    //
    // This is important because when freeing an unmarked object, we don't always
    // know how much memory it is using. For example, when freeing an instance,
    // we need to know its class to know how big it is, but its class may have
    // already been freed.
    vm->bytesAllocated = 0;

    if (vm->block != NULL) {
        mochiGrayObj(vm, (Obj*)vm->block);
    }
    // The current fiber.
    if (vm->fiber != NULL) {
        mochiGrayObj(vm, (Obj*)vm->fiber);
    }

    // Now that we have grayed the roots, do a depth-first search over all of the
    // reachable objects.
    mochiBlackenObjects(vm);

    // Collect the white objects.
    unsigned long freed = 0;
    unsigned long reachable = 0;
    Obj** obj = &vm->objects;
    while (*obj != NULL) {
        if (!((*obj)->isMarked)) {
            // This object wasn't reached, so remove it from the list and free it.
            Obj* unreached = *obj;
            *obj = unreached->next;
            mochiFreeObj(vm, unreached);
            freed += 1;
        } else {
            // This object was reached, so unmark it (for the next GC) and move on to
            // the next.
            (*obj)->isMarked = false;
            obj = &(*obj)->next;
            reachable += 1;
        }
    }

    // Calculate the next gc point, this is the current allocation plus
    // a configured percentage of the current allocation.
    vm->nextGC = vm->bytesAllocated + ((vm->bytesAllocated * vm->config.heapGrowthPercent) / 100);
    if (vm->nextGC < vm->config.minHeapSize) vm->nextGC = vm->config.minHeapSize;

#if MOCHIVM_DEBUG_TRACE_MEMORY || MOCHIVM_DEBUG_TRACE_GC
    double elapsed = ((double)clock() / CLOCKS_PER_SEC) - startTime;
    // Explicit cast because size_t has different sizes on 32-bit and 64-bit and
    // we need a consistent type for the format string.
    printf("GC %lu reachable, %lu freed. Took %.3fms.\nGC %lu before, %lu after (~%lu collected), next at %lu.\n",
            reachable,
            freed,
            elapsed*1000.0,
            (unsigned long)before,
            (unsigned long)vm->bytesAllocated,
            (unsigned long)(before - vm->bytesAllocated),
            (unsigned long)vm->nextGC);
#endif
}

int addConstant(MochiVM* vm, Value value) {
    ASSERT(vm->fiber != NULL, "Cannot add a constant without a fiber already assigned to the VM.");
    if (IS_OBJ(value)) { mochiFiberPushRoot(vm->fiber, AS_OBJ(value)); }
    mochiValueBufferWrite(vm, &vm->block->constants, value);
    if (IS_OBJ(value)) { mochiFiberPopRoot(vm->fiber); }
    return vm->block->constants.count - 1;
}

void writeChunk(MochiVM* vm, uint8_t instr, int line) {
    mochiByteBufferWrite(vm, &vm->block->code, instr);
    mochiIntBufferWrite(vm, &vm->block->lines, line);
}

void writeLabel(MochiVM* vm, int byteIndex, int labelLength, const char* labelText) {
    mochiIntBufferWrite(vm, &vm->block->labelIndices, byteIndex);
    ObjString* str = copyString(labelText, labelLength, vm);
    mochiFiberPushRoot(vm->fiber, (Obj*)str);
    mochiValueBufferWrite(vm, &vm->block->labels, OBJ_VAL(str));
    mochiFiberPopRoot(vm->fiber);
}

char* getLabel(MochiVM* vm, int byteIndex) {
    for (int i = 0; i < vm->block->labelIndices.count; i++) {
        if (vm->block->labelIndices.data[i] == byteIndex) {
            return AS_CSTRING(vm->block->labels.data[i]);
        }
    }
    return NULL;
}

int mochiAddForeign(MochiVM* vm, MochiVMForeignMethodFn fn) {
    mochiForeignFunctionBufferWrite(vm, &vm->foreignFns, fn);
    return vm->foreignFns.count - 1;
}

Heap* mochiNewHeap(MochiVM* vm)
{
    Heap* heap = ALLOCATE(vm, Heap);
    heap->capacity = 0;
    heap->count = 0;
    heap->entries = NULL;
    return heap;
}

static inline uint32_t hashBits(uint64_t hash)
{
    // From v8's ComputeLongHash() which in turn cites:
    // Thomas Wang, Integer Hash Functions.
    // http://www.concentric.net/~Ttwang/tech/inthash.htm
    hash = ~hash + (hash << 18);  // hash = (hash << 18) - hash - 1;
    hash = hash ^ (hash >> 31);
    hash = hash * 21;  // hash = (hash + (hash << 2)) + (hash << 4);
    hash = hash ^ (hash >> 11);
    hash = hash + (hash << 6);
    hash = hash ^ (hash >> 22);
    return (uint32_t)(hash & 0x3fffffff);
}

// Looks for an entry with [key] in an array of [capacity] [entries].
//
// If found, sets [result] to point to it and returns `true`. Otherwise,
// returns `false` and points [result] to the entry where the key/value pair
// should be inserted.
static bool findEntry(HeapEntry* entries, uint32_t capacity, HeapKey key, HeapEntry** result)
{
    // If there is no entry array (an empty map), we definitely won't find it.
    if (capacity == 0) { return false; }

    // Figure out where to insert it in the table. Use open addressing and
    // basic linear probing.
    uint32_t startIndex = hashBits(key) % capacity;
    uint32_t index = startIndex;

    // If we pass a tombstone and don't end up finding the key, its entry will
    // be re-used for the insert.
    HeapEntry* tombstone = NULL;

    // Walk the probe sequence until we've tried every slot.
    do
    {
        HeapEntry* entry = &entries[index];

        // 0 or 1 signifies an empty slot.
        if (entry->key < 2)
        {
            // If we found an empty slot, the key is not in the table. If we found a
            // slot that contains a deleted key, we have to keep looking.
            if (entry->value == 0)
            {
                // We found an empty non-deleted slot, so we've reached the end of the probe
                // sequence without finding the key.
                *result = tombstone != NULL ? tombstone : entry;
                return false;
            }
            else
            {
                // We found a tombstone. We need to keep looking in case the key is after it.
                if (tombstone == NULL) { tombstone = entry; }
            }
        }
        else if (entry->key == key)
        {
            // We found the key.
            *result = entry;
            return true;
        }

        // Try the next slot.
        index = (index + 1) % capacity;
    }
    while (index != startIndex);

    // If we get here, the table is full of tombstones. Return the first one we found.
    ASSERT(tombstone != NULL, "Map should have tombstones or empty entries.");
    *result = tombstone;
    return false;
}

// Inserts [key] and [value] in the array of [entries] with the given
// [capacity].
//
// Returns `true` if this is the first time [key] was added to the map.
static bool insertEntry(HeapEntry* entries, uint32_t capacity, HeapKey key, Value value)
{
    ASSERT(entries != NULL, "Should ensure capacity before inserting.");
    
    HeapEntry* entry;
    if (findEntry(entries, capacity, key, &entry))
    {
        // Already present, so just replace the value.
        entry->value = value;
        return false;
    }
    else
    {
        entry->key = key;
        entry->value = value;
        return true;
    }
}

// Updates [heap]'s entry array to [capacity].
static void resizeHeap(MochiVM* vm, Heap* heap, uint32_t capacity)
{
    // Create the new empty hash table.
    HeapEntry* entries = ALLOCATE_ARRAY(vm, HeapEntry, capacity);
    for (uint32_t i = 0; i < capacity; i++)
    {
        entries[i].key = 0;
        entries[i].value = FALSE_VAL;
    }

    // Re-add the existing entries.
    if (heap->capacity > 0)
    {
        for (uint32_t i = 0; i < heap->capacity; i++)
        {
            HeapEntry* entry = &heap->entries[i];
            
            // Don't copy empty entries or tombstones.
            if (entry->key < 2) { continue; }

            insertEntry(entries, capacity, entry->key, entry->value);
        }
    }

    // Replace the array.
    DEALLOCATE(vm, heap->entries);
    heap->entries = entries;
    heap->capacity = capacity;
}

bool mochiHeapGet(Heap* heap, HeapKey key, Value* out)
{
    HeapEntry* entry;
    bool found = findEntry(heap->entries, heap->capacity, key, &entry);
    ASSERT(out != NULL, "Cannot pass null pointer for Value out in mochiHeapGet.");
    *out = found ? entry->value : FALSE_VAL;
    return found;
}

void mochiHeapSet(MochiVM* vm, Heap* heap, HeapKey key, Value value)
{
    // If the map is getting too full, make room first.
    if (heap->count + 1 > heap->capacity * HEAP_LOAD_PERCENT / 100)
    {
        // Figure out the new hash table size.
        uint32_t capacity = heap->capacity * HEAP_GROW_FACTOR;
        if (capacity < HEAP_MIN_CAPACITY) { capacity = HEAP_MIN_CAPACITY; }

        resizeHeap(vm, heap, capacity);
    }

    if (insertEntry(heap->entries, heap->capacity, key, value))
    {
        // A new key was added.
        heap->count++;
    }
}

void mochiHeapClear(MochiVM* vm, Heap* heap)
{
    DEALLOCATE(vm, heap->entries);
    heap->entries = NULL;
    heap->capacity = 0;
    heap->count = 0;
}

bool mochiHeapTryRemove(MochiVM* vm, Heap* heap, HeapKey key)
{
    HeapEntry* entry;
    if (!findEntry(heap->entries, heap->capacity, key, &entry)) { return false; }

    // Remove the entry from the heap. Set this key to 1, which marks it as a
    // deleted slot. When searching for a key, we will stop on empty slots, but
    // continue past deleted slots.
    entry->key = 1;
    entry->value = FALSE_VAL;

    heap->count--;

    if (heap->count == 0)
    {
        // Removed the last item, so free the array.
        mochiHeapClear(vm, heap);
    }
    else if (heap->capacity > HEAP_MIN_CAPACITY &&
            heap->count < heap->capacity / HEAP_GROW_FACTOR * HEAP_LOAD_PERCENT / 100)
    {
        uint32_t capacity = heap->capacity / HEAP_GROW_FACTOR;
        if (capacity < HEAP_MIN_CAPACITY) { capacity = HEAP_MIN_CAPACITY; }

        // The heap is getting empty, so shrink the entry array back down.
        // TODO: Should we do this less aggressively than we grow?
        resizeHeap(vm, heap, capacity);
    }

    return true;
}