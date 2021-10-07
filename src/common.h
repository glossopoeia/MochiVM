#ifndef zhenzhu_common_h
#define zhenzhu_common_h

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

// These flags let you control some details of the interpreter's implementation.
// Usually they trade-off a bit of portability for speed. They default to the
// most efficient behavior.

// Because Zhenzhu is intended as a target for the statically-typed Boba
// language, we can make guarantees that runtime type errors will not
// occur, enabling a more efficient representation of data at runtime.
// However, this may be less portable in the future, so we also make
// available a version that uses standard C tagged unions. Pointer
// tagging defaults to on for efficiency on common hardware.
#ifndef ZHENZHU_POINTER_TAGGING
    #define ZHENZHU_POINTER_TAGGING 1
#endif

// We also make available a representation for NaN tagging/boxing,
// so we can run experiments to see which is more useful in certain
// scenarios. For example, an application that uses lots of floating
// point arithmetic could benefit from NaN tagging instead of pointer
// tagging.
#ifndef ZHENZHU_POINTER_TAGGING
    #ifndef ZHENZHU_NAN_TAGGING
        #define ZHENZHU_NAN_TAGGING 1
    #endif
#endif

// If true, the VM's interpreter loop uses computed gotos. See this for more:
// http://gcc.gnu.org/onlinedocs/gcc-3.1.1/gcc/Labels-as-Values.html
// Enabling this speeds up the main dispatch loop a bit, but requires compiler
// support.
// see https://bullno1.com/blog/switched-goto for alternative
// Defaults to true on supported compilers.
#ifndef ZHENZHU_COMPUTED_GOTO
    #if defined(_MSC_VER) && !defined(__clang__)
        // No computed gotos in Visual Studio.
        #define ZHENZHU_COMPUTED_GOTO 0
    #else
        #define ZHENZHU_COMPUTED_GOTO 1
    #endif
#endif

// The VM includes a number of optional 'batteries'. You can choose to include
// these or not. By default, they are all available. To disable one, set the
// corresponding `ZHENZHU_BATTERY_<name>` define to `0`.
#ifndef ZHENZHU_BATTERY_UV
    #define ZHENZHU_BATTERY_UV 1  // LibUV included by default to support concurrent system ops
#endif

#ifndef ZHENZHU_BATTERY_SDL
    #define ZHENZHU_BATTERY_SDL 0   // SDL2 included by default for windowing, graphics, audio, input, etc.
#endif

// These flags are useful for debugging and hacking on Zhenzhu itself. They are not
// intended to be used for production code. They default to off.

#ifndef 

#define DEBUG_TRACE_EXECUTION

#endif