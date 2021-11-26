#include <stdlib.h>
#include <string.h>

#include "memory.h"

void* mochiReallocate(MochiVM* vm, void* memory, size_t oldSize, size_t newSize) {

    // TODO: while this method is protected from starting multiple GC cycles simultaneously
    // thanks to a simple lock, it is not yet fully thread safe with respect to moving around
    // data. Need thread synchronization before declaring multithread safe GC!

    // Only lock if we're allocating new memory, since that's the only thing that
    // will prompt a new GC cycle.
    if (newSize > 0) {
        mtx_lock(&vm->allocLock);
#if MOCHIVM_DEBUG_TRACE_MEMORY
        printf("Lock acquired.\n");
#endif
    }

    // If new bytes are being allocated, add them to the total count. If objects
    // are being completely deallocated, we don't track that (since we don't
    // track the original size). Instead, that will be handled while marking
    // during the next GC.
    size_t newHeapSize = vm->bytesAllocated + (newSize - oldSize);

#if MOCHIVM_DEBUG_TRACE_MEMORY
    // Explicit cast because size_t has different sizes on 32-bit and 64-bit and
    // we need a consistent type for the format string.
    printf("reallocate %p %lu -> %lu, total %lu -> %lu\n", memory, (unsigned long)oldSize, (unsigned long)newSize,
           (unsigned long)vm->bytesAllocated, (unsigned long)newHeapSize);
#endif

    vm->bytesAllocated = newHeapSize;

#if MOCHIVM_DEBUG_GC_STRESS
    // Since collecting calls this function to free things, make sure we don't
    // recurse.
    if (newSize > 0)
        mochiCollectGarbage(vm);
#else
    if (newSize > 0 && newHeapSize > vm->nextGC)
        mochiCollectGarbage(vm);
#endif

    void* res = vm->config.reallocateFn(memory, newSize, vm->config.userData);

    if (newSize > 0) {
        mtx_unlock(&vm->allocLock);
#if MOCHIVM_DEBUG_TRACE_MEMORY
        printf("Lock released.\n");
#endif
    }
    
    return res;
}

// From: http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2Float
int mochiPowerOf2Ceil(int n) {
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n++;
    return n;
}