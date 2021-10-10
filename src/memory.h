#ifndef zhenzhu_memory_h
#define zhenzhu_memory_h

#include "common.h"
#include "object.h"
#include "vm.h"

// Use the VM's allocator to allocate an object of [type].
#define ALLOCATE(vm, type)                                                     \
    ((type*)zzReallocate(vm, NULL, 0, sizeof(type)))

// Use the VM's allocator to allocate an object of [mainType] containing a
// flexible array of [count] objects of [arrayType].
#define ALLOCATE_FLEX(vm, mainType, arrayType, count)                          \
    ((mainType*)zzReallocate(vm, NULL, 0,                                      \
        sizeof(mainType) + sizeof(arrayType) * (count)))

// Use the VM's allocator to allocate an array of [count] elements of [type].
#define ALLOCATE_ARRAY(vm, type, count)                                        \
    ((type*)zzReallocate(vm, NULL, 0, sizeof(type) * (count)))

// Use the VM's allocator to free the previously allocated memory at [pointer].
#define DEALLOCATE(vm, pointer) zzReallocate(vm, pointer, 0, 0)

// A generic allocation function that handles all explicit memory management.
// It's used like so:
//
// - To allocate new memory, [memory] is NULL and [oldSize] is zero. It should
//   return the allocated memory or NULL on failure.
//
// - To attempt to grow an existing allocation, [memory] is the memory,
//   [oldSize] is its previous size, and [newSize] is the desired size.
//   It should return [memory] if it was able to grow it in place, or a new
//   pointer if it had to move it.
//
// - To shrink memory, [memory], [oldSize], and [newSize] are the same as above
//   but it will always return [memory].
//
// - To free memory, [memory] will be the memory to free and [newSize] and
//   [oldSize] will be zero. It should return NULL.
void* zzReallocate(ZZVM* vm, void* memory, size_t oldSize, size_t newSize);

// Returns the smallest power of two that is equal to or greater than [n].
int zzPowerOf2Ceil(int n);

#endif