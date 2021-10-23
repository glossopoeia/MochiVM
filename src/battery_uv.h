#ifndef mochivm_battery_uv_h

#include "mochivm.h"

// Initializes a new timer object and pushes it to the stack.
// Stack effect:
//      a...        --  a... Timer
void uvmochiNewTimer(MochiVM* vm, ObjFiber* fiber);
// Properly releases the resources associated with the timer object on top of the stack, and pops it.
// WARNING: if multiple references to the timer exist, calling this function on those references will
// cause double-free problems. Accessing other references after one has been close is the same as
// accessing freed memory.
// Stack effect:
//      a... Timer  --  a...
void uvmochiCloseTimer(MochiVM* vm, ObjFiber* fiber);
void uvmochiTimerStart(MochiVM* vm, ObjFiber* fiber);
void uvmochiTimerStop(MochiVM* vm, ObjFiber* fiber);
void uvmochiTimerSetRepeat(MochiVM* vm, ObjFiber* fiber);
void uvmochiTimerAgain(MochiVM* vm, ObjFiber* fiber);

#endif