#ifndef mochivm_common_h
#define mochivm_common_h

// Significant portions of this header are taken from the Wren project, including
// the well-worded comments. See here: https://github.com/wren-lang/wren

// This header contains macros and defines used across the entire Zhenzhu
// implementation. In particular, it contains "configuration" defines that
// control how Zhenzhu works. Some of these are only used while hacking on Zhenzhu
// itself.
//
// This header is *not* intended to be included by code outside of Zhenzhu itself.

// Zhenzhu pervasively uses the C99 integer types (uint16_t, etc.) along with some
// of the associated limit constants (UINT32_MAX, etc.). The constants are not
// part of standard C++, so aren't included by default by C++ compilers when you
// include <stdint> unless __STDC_LIMIT_MACROS is defined.
#define __STDC_LIMIT_MACROS
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

// The Microsoft compiler does not support the "inline" modifier when compiling
// as plain C.
#if defined( _MSC_VER ) && !defined(__cplusplus)
    #define inline _inline
#endif

// These flags let you control some details of the interpreter's implementation.
// Usually they trade-off a bit of portability for speed. They default to the
// most efficient behavior.

// Because Zhenzhu is intended as a target for the statically-typed Boba
// language, we can make guarantees that runtime type errors will not
// occur, enabling a more efficient representation of data at runtime.
// However, this may be less portable in the future, so we also make
// available a version that uses standard C tagged unions. Pointer
// tagging defaults to on for efficiency on common hardware.
#ifndef MOCHIVM_POINTER_TAGGING
    #define MOCHIVM_POINTER_TAGGING 0
#endif

// We also make available a representation for NaN tagging/boxing,
// so we can run experiments to see which is more useful in certain
// scenarios. For example, an application that uses lots of floating
// point arithmetic could benefit from NaN tagging instead of pointer
// tagging.
#ifndef MOCHIVM_NAN_TAGGING
    #define MOCHIVM_NAN_TAGGING 1
#endif

#if MOCHIVM_POINTER_TAGGING == 1 && MOCHIVM_NAN_TAGGING == 1
    #error Pointer tagging and NaN tagging cannot both be enabled.
#endif

// If true, the VM's interpreter loop uses computed gotos. See this for more:
// http://gcc.gnu.org/onlinedocs/gcc-3.1.1/gcc/Labels-as-Values.html
// Enabling this speeds up the main dispatch loop a bit, but requires compiler
// support.
// see https://bullno1.com/blog/switched-goto for alternative
// Defaults to true on supported compilers.
#ifndef MOCHIVM_COMPUTED_GOTO
    #if defined(_MSC_VER) && !defined(__clang__)
        // No computed gotos in Visual Studio.
        #define MOCHIVM_COMPUTED_GOTO 0
    #else
        #define MOCHIVM_COMPUTED_GOTO 1
    #endif
#endif

// The VM includes a number of optional 'batteries'. You can choose to include
// these or not. By default, they are all available. To disable one, set the
// corresponding `MOCHIVM_BATTERY_<name>` define to `0`.
#ifndef MOCHIVM_BATTERY_UV
    #define MOCHIVM_BATTERY_UV 0  // LibUV included by default to support concurrent system ops
#endif

#ifndef MOCHIVM_BATTERY_SDL
    #define MOCHIVM_BATTERY_SDL 0   // SDL2 included by default for windowing, graphics, audio, input, etc.
#endif

// These flags are useful for debugging and hacking on Zhenzhu itself. They are not
// intended to be used for production code. They default to off.

// Run garbage collection before every allocation.
#define MOCHIVM_DEBUG_GC_STRESS 1

// Log all memory operations.
#define MOCHIVM_DEBUG_TRACE_MEMORY 1

// Log all garbage collections.
#define MOCHIVM_DEBUG_TRACE_GC 1

// Display all the input bytecode before beginning execution.
#define MOCHIVM_DEBUG_DUMP_BYTECODE 1

// Log VM state and current instruction before every executed instruction.
#define MOCHIVM_DEBUG_TRACE_EXECUTION 1

// Log fiber value stack state on every instruction execution.
#define MOCHIVM_DEBUG_TRACE_VALUE_STACK 1

// Log fiber frame stack state on every instruction execution.
#define MOCHIVM_DEBUG_TRACE_FRAME_STACK 1

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
    void mochi##name##BufferInit(name##Buffer* buffer);                           \
    void mochi##name##BufferClear(MochiVM* vm, name##Buffer* buffer);                \
    void mochi##name##BufferFill(MochiVM* vm, name##Buffer* buffer, type data,       \
                                int count);                                    \
    void mochi##name##BufferWrite(MochiVM* vm, name##Buffer* buffer, type data)

// This should be used once for each type instantiation, somewhere in a .c file.
#define DEFINE_BUFFER(name, type)                                              \
    void mochi##name##BufferInit(name##Buffer* buffer)                            \
    {                                                                          \
      buffer->data = NULL;                                                     \
      buffer->capacity = 0;                                                    \
      buffer->count = 0;                                                       \
    }                                                                          \
                                                                               \
    void mochi##name##BufferClear(MochiVM* vm, name##Buffer* buffer)                 \
    {                                                                          \
      mochiReallocate(vm, buffer->data, 0, 0);                                    \
      mochi##name##BufferInit(buffer);                                            \
    }                                                                          \
                                                                               \
    void mochi##name##BufferFill(MochiVM* vm, name##Buffer* buffer, type data,       \
                                int count)                                     \
    {                                                                          \
      if (buffer->capacity < buffer->count + count)                            \
      {                                                                        \
        int capacity = mochiPowerOf2Ceil(buffer->count + count);                  \
        buffer->data = (type*)mochiReallocate(vm, buffer->data,                   \
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
    void mochi##name##BufferWrite(MochiVM* vm, name##Buffer* buffer, type data)      \
    {                                                                          \
      mochi##name##BufferFill(vm, buffer, data, 1);                               \
    }

// Assertions are used to validate program invariants. They indicate things the
// program expects to be true about its internal state during execution. If an
// assertion fails, there is a bug in Zhenzhu.
//
// Assertions add significant overhead, so are only enabled in debug builds.
#ifdef DEBUG

    #include <stdio.h>

    #define ASSERT(condition, message)                                           \
        do                                                                       \
        {                                                                        \
            if (!(condition))                                                    \
            {                                                                    \
                fprintf(stderr, "[%s:%d] Assert failed in %s(): %s\n",           \
                    __FILE__, __LINE__, __func__, message);                      \
                abort();                                                         \
            }                                                                    \
        } while (false)

    // Indicates that we know execution should never reach this point in the
    // program. In debug mode, we assert this fact because it's a bug to get here.
    //
    // In release mode, we use compiler-specific built in functions to tell the
    // compiler the code can't be reached. This avoids "missing return" warnings
    // in some cases and also lets it perform some optimizations by assuming the
    // code is never reached.
    #define UNREACHABLE()                                                        \
        do                                                                       \
        {                                                                        \
            fprintf(stderr, "[%s:%d] This code should not be reached in %s()\n", \
                __FILE__, __LINE__, __func__);                                   \
            abort();                                                             \
        } while (false)

#else

    #define ASSERT(condition, message) do { } while (false)

    // Tell the compiler that this part of the code will never be reached.
    #if defined( _MSC_VER )
        #define UNREACHABLE() __assume(0)
    #elif (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5))
        #define UNREACHABLE() __builtin_unreachable()
    #else
        #define UNREACHABLE()
    #endif

#endif

#endif