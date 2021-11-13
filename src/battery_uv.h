#ifndef mochivm_battery_uv_h
#define mochivm_battery_uv_h

#include "mochivm.h"

// Initializes a new timer object and pushes it to the stack.
//      a...        --  a... Timer
void uvmochiNewTimer(MochiVM* vm, ObjFiber* fiber);
// Properly releases the resources associated with the timer object on top of the stack, and pops it.
// WARNING: if multiple references to the timer exist, calling this function on those references will
// cause double-free problems. Accessing other references after one has been close is the same as
// accessing freed memory.
//      a... Timer  --  a...
void uvmochiCloseTimer(MochiVM* vm, ObjFiber* fiber);
// Starts the timer on top of the stack with the given duration. The timer will execute the callback
// in the third stack slot when the duration has elapsed. Suspends the current fiber until the duration
// has elapsed. Places the timer reference on top of the stack for the callback.
//      a... (a... Timer -> c...) U64 Timer  --  c...
void uvmochiTimerStart(MochiVM* vm, ObjFiber* fiber);
void uvmochiTimerStop(MochiVM* vm, ObjFiber* fiber);
void uvmochiTimerSetRepeat(MochiVM* vm, ObjFiber* fiber);
void uvmochiTimerAgain(MochiVM* vm, ObjFiber* fiber);

#endif