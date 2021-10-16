#ifndef zhenzhu_battery_uv_h

#include "zhenzhu.h"

// Initializes a new timer object and pushes it to the stack.
// Stack effect:
//      a...        --  a... Timer
void uvzzNewTimer(ZZVM* vm, ObjFiber* fiber);
// Properly releases the resources associated with the timer object on top of the stack, and pops it.
// WARNING: if multiple references to the timer exist, calling this function on those references will
// cause double-free problems. Accessing other references after one has been close is the same as
// accessing freed memory.
// Stack effect:
//      a... Timer  --  a...
void uvzzCloseTimer(ZZVM* vm, ObjFiber* fiber);
void uvzzTimerStart(ZZVM* vm, ObjFiber* fiber);
void uvzzTimerStop(ZZVM* vm, ObjFiber* fiber);
void uvzzTimerSetRepeat(ZZVM* vm, ObjFiber* fiber);
void uvzzTimerAgain(ZZVM* vm, ObjFiber* fiber);

#endif