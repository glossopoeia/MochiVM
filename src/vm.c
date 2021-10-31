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

#if MOCHIVM_BATTERY_UV
    mochiAddForeign(vm, uvmochiNewTimer);
    mochiAddForeign(vm, uvmochiCloseTimer);
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

int mochiAddForeign(MochiVM* vm, MochiVMForeignMethodFn fn) {
    mochiForeignFunctionBufferWrite(vm, &vm->foreignFns, fn);
    return vm->foreignFns.count - 1;
}