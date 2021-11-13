#include "battery_sdl.h"
#include "memory.h"
#include "debug.h"
#include <SDL.h>

void sdlmochiInit(MochiVM* vm, ObjFiber* fiber) {
    int subsystems = AS_I32(mochiFiberPopValue(fiber));
    int result = SDL_Init(subsystems);
    printf("SDL init result: %d\n", result);
    mochiFiberPushValue(fiber, I32_VAL(vm, result));
}

void sdlmochiQuit(MochiVM* vm, ObjFiber* fiber) {
    SDL_Quit();
}