#ifndef zhenzhu_h
#define zhenzhu_h

#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>

// The Zhenzhu semantic version number components.
#define ZHENZHU_VERSION_MAJOR 0
#define ZHENZHU_VERSION_MINOR 1
#define ZHENZHU_VERSION_PATCH 0

// A human-friendly string representation of the version.
#define ZHENZHU_VERSION_STRING "0.1.0"

// A monotonically increasing numeric representation of the version number. Use
// this if you want to do range checks over versions.
#define ZHENZHU_VERSION_NUMBER (ZHENZHU_VERSION_MAJOR * 1000000 +                    \
                                ZHENZHU_VERSION_MINOR * 1000 +                       \
                                ZHENZHU_VERSION_PATCH)

#ifndef ZHENZHU_API
  #if defined(_MSC_VER) && defined(ZHENZHU_API_DLLEXPORT)
    #define ZHENZHU_API __declspec( dllexport )
  #else
    #define ZHENZHU_API
  #endif
#endif

// A single virtual machine for executing Zhenzhu byte code.
//
// Zhenzhu has no global state, so all state stored by a running interpreter lives
// here.
typedef struct ZZVM ZZVM;
typedef struct ObjFiber ObjFiber;

// A generic allocation function that handles all explicit memory management
// used by Zhenzhu. It's used like so:
//
// - To allocate new memory, [memory] is NULL and [newSize] is the desired
//   size. It should return the allocated memory or NULL on failure.
//
// - To attempt to grow an existing allocation, [memory] is the memory, and
//   [newSize] is the desired size. It should return [memory] if it was able to
//   grow it in place, or a new pointer if it had to move it.
//
// - To shrink memory, [memory] and [newSize] are the same as above but it will
//   always return [memory].
//
// - To free memory, [memory] will be the memory to free and [newSize] will be
//   zero. It should return NULL.
typedef void* (*ZhenzhuReallocateFn)(void* memory, size_t newSize, void* userData);

// A function callable from Zhenzhu code, but implemented in C.
typedef void (*ZhenzhuForeignMethodFn)(ZZVM* vm);

// Reports an error to the user.
//
// A runtime error is reported by calling this once with no [module] or [line], and the runtime error's
// [message]. After that, a series of calls are made for each line in the stack trace. Each of those has the resolved
// [module] and [line] where the method or function is defined and [message] is
// the name of the method or function.
typedef void (*ZhenzhuErrorFn)(ZZVM* vm, const char* module, int line, const char* message);

typedef struct {
    // The callback Zhenzhu will use to allocate, reallocate, and deallocate memory.
    //
    // If `NULL`, defaults to a built-in function that uses `realloc` and `free`.
    ZhenzhuReallocateFn reallocateFn;

    // The callback Zhenzhu uses to report errors.
    //
    // When an error occurs, this will be called with the module name, line
    // number, and an error message. If this is `NULL`, Zhenzhu doesn't report any
    // errors.
    ZhenzhuErrorFn errorFn;

    // The maximum number of values the VM will allow in a fiber's value stack.
    // If zero, defaults to 128.
    int valueStackCapacity;

    // The maximum number of frames the VM will allow in a fiber's frame stack.
    // If zero, defaults to 512.
    int frameStackCapacity;

    // The number of bytes Zhenzhu will allocate before triggering the first garbage
    // collection.
    //
    // If zero, defaults to 10MB.
    size_t initialHeapSize;

    // After a collection occurs, the threshold for the next collection is
    // determined based on the number of bytes remaining in use. This allows Zhenzhu
    // to shrink its memory usage automatically after reclaiming a large amount
    // of memory.
    //
    // This can be used to ensure that the heap does not get too small, which can
    // in turn lead to a large number of collections afterwards as the heap grows
    // back to a usable size.
    //
    // If zero, defaults to 1MB.
    size_t minHeapSize;

    // Zhenzhu will resize the heap automatically as the number of bytes
    // remaining in use after a collection changes. This number determines the
    // amount of additional memory Zhenzhu will use after a collection, as a
    // percentage of the current heap size.
    //
    // For example, say that this is 50. After a garbage collection, when there
    // are 400 bytes of memory still in use, the next collection will be triggered
    // after a total of 600 bytes are allocated (including the 400 already in
    // use.)
    //
    // Setting this to a smaller number wastes less memory, but triggers more
    // frequent garbage collections.
    //
    // If zero, defaults to 50.
    int heapGrowthPercent;

    // User-defined data associated with the VM.
    void* userData;

} ZhenzhuConfiguration;

typedef enum
{
    ZHENZHU_RESULT_SUCCESS,
    ZHENZHU_RESULT_RUNTIME_ERROR
} ZhenzhuInterpretResult;

// Get the current Zhenzhu version number.
//
// Can be used to range checks over versions.
ZHENZHU_API int zzGetVersionNumber();

// Initializes [configuration] with all of its default values.
//
// Call this before setting the particular fields you care about.
ZHENZHU_API void zzInitConfiguration(ZhenzhuConfiguration* configuration);

// Creates a new Zhenzhu virtual machine using the given [configuration]. Zhenzhu
// will copy the configuration data, so the argument passed to this can be
// freed after calling this. If [configuration] is `NULL`, uses a default
// configuration.
ZHENZHU_API ZZVM* zzNewVM(ZhenzhuConfiguration* configuration);

// Disposes of all resources is use by [vm], which was previously created by a
// call to [zzNewVM].
ZHENZHU_API void zzFreeVM(ZZVM* vm);

// Immediately run the garbage collector to free unused memory.
ZHENZHU_API void zzCollectGarbage(ZZVM* vm);

// Runs [source], a string of Zhenzhu source code in a new fiber in [vm] in the
// context of resolved [module].
ZHENZHU_API ZhenzhuInterpretResult zzInterpret(ZZVM* vm, ObjFiber* fiber);

#endif