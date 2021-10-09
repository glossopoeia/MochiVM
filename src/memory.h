#ifndef zhenzhu_memory_h
#define zhenzhu_memory_h

#include "common.h"
#include "object.h"
#include "vm.h"

#define ALLOCATE(type, count) (type*)zzReallocate(NULL, 0, sizeof(type) * (count))

#define FREE(type, pointer) zzReallocate(pointer, sizeof(type), 0)

#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(type, pointer, oldCount, newCount) \
    (type*)zzReallocate(pointer, sizeof(type) * (oldCount), \
        sizeof(type) * (newCount))

#define FREE_ARRAY(type, pointer, oldCount) \
    zzReallocate(pointer, sizeof(type) * (oldCount), 0)

// We need buffers of a few different types. To avoid lots of casting between
// void* and back, we'll use the preprocessor as a poor man's generics and let
// it generate a few type-specific ones.
#define DECLARE_BUFFER(name, type)                                             \
    typedef struct                                                             \
    {                                                                          \
        type* data;                                                            \
        int count;                                                             \
        int capacity;                                                          \
    } name##Buffer;                                                            \
    void zz##name##BufferInit(name##Buffer* buffer);                           \
    void zz##name##BufferClear(ZZVM* vm, name##Buffer* buffer);                \
    void zz##name##BufferFill(ZZVM* vm, name##Buffer* buffer, type data,       \
                                int count);                                    \
    void zz##name##BufferWrite(ZZVM* vm, name##Buffer* buffer, type data)

// This should be used once for each type instantiation, somewhere in a .c file.
#define DEFINE_BUFFER(name, type)                                              \
    void zz##name##BufferInit(name##Buffer* buffer)                            \
    {                                                                          \
      buffer->data = NULL;                                                     \
      buffer->capacity = 0;                                                    \
      buffer->count = 0;                                                       \
    }                                                                          \
                                                                               \
    void zz##name##BufferClear(ZZVM* vm, name##Buffer* buffer)                 \
    {                                                                          \
      zzReallocate(vm, buffer->data, 0, 0);                                    \
      zz##name##BufferInit(buffer);                                            \
    }                                                                          \
                                                                               \
    void zz##name##BufferFill(ZZVM* vm, name##Buffer* buffer, type data,       \
                                int count)                                     \
    {                                                                          \
      if (buffer->capacity < buffer->count + count)                            \
      {                                                                        \
        int capacity = zzPowerOf2Ceil(buffer->count + count);                  \
        buffer->data = (type*)zzReallocate(vm, buffer->data,                   \
            buffer->capacity * sizeof(type), capacity * sizeof(type));         \
        buffer->capacity = capacity;                                           \
      }                                                                        \
                                                                               \
      for (int i = 0; i < count; i++)                                          \
      {                                                                        \
        buffer->data[buffer->count++] = data;                                  \
      }                                                                        \
    }                                                                          \
                                                                               \
    void zz##name##BufferWrite(ZZVM* vm, name##Buffer* buffer, type data)      \
    {                                                                          \
      zz##name##BufferFill(vm, buffer, data, 1);                               \
    }

// If newSize is 0, frees the pointed-to memory. Otherwise reallocates
// at the new specified size. Early exit if memory cannot be reallocated.
void* zzReallocate(void* pointer, size_t oldSize, size_t newSize);

// Returns the smallest power of two that is equal to or greater than [n].
int zzPowerOf2Ceil(int n);

#endif