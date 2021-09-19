#ifndef zhenzhu_memory_h
#define zhenzhu_memory_h

#include "common.h"
#include "object.h"
#include "vm.h"

#define ALLOCATE(type, count) (type*)reallocate(NULL, 0, sizeof(type) * (count))

#define FREE(type, pointer) reallocate(pointer, sizeof(type), 0)

#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(type, pointer, oldCount, newCount) \
    (type*)reallocate(pointer, sizeof(type) * (oldCount), \
        sizeof(type) * (newCount))

#define FREE_ARRAY(type, pointer, oldCount) \
    reallocate(pointer, sizeof(type) * (oldCount), 0)

// If newSize is 0, frees the pointed-to memory. Otherwise reallocates
// at the new specified size. Early exit if memory cannot be reallocated.
void* reallocate(void* pointer, size_t oldSize, size_t newSize);

void freeObjects(VM* vm);
void freeObject(Obj* object);

#endif