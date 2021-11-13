#ifndef mochivm_battery_sdl_h
#define mochivm_battery_sdl_h

#include "mochivm.h"

// Initialize the SDL library.
// See https://wiki.libsdl.org/SDL_Init
//     a... I32 --> a... I32
void sdlmochiInit(MochiVM* vm, ObjFiber* fiber);
// Clean up all initialized subsystems.
// See https://wiki.libsdl.org/SDL_Quit
//     a... --> a...
void sdlmochiQuit(MochiVM* vm, ObjFiber* fiber);

#endif