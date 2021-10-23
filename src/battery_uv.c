#include "battery_uv.h"
#include "vm.h"
#include "uv.h"

void uvmochiNewTimer(MochiVM* vm, ObjFiber* fiber) {
    uv_timer_t* timer = vm->config.reallocateFn(NULL, sizeof(uv_timer_t*), vm->config.userData);
    uv_timer_init(uv_default_loop(), timer);

    ObjCPointer* ptr = mochiNewCPointer(vm, timer);
    mochiFiberPushValue(fiber, OBJ_VAL(ptr));
}

void uvmochiCloseTimer(MochiVM* vm, ObjFiber* fiber) {
    ObjCPointer* ptr = (ObjCPointer*)AS_OBJ(mochiFiberPopValue(fiber));
    uv_timer_stop((uv_timer_t*)ptr->pointer);
    vm->config.reallocateFn(ptr->pointer, 0, vm->config.userData);
}

void uvmochiTimerStart(MochiVM* vm, ObjFiber* fiber) {
    
}

void uvmochiTimerStop(MochiVM* vm, ObjFiber* fiber) {

}

void uvmochiTimerSetRepeat(MochiVM* vm, ObjFiber* fiber) {

}

void uvmochiTimerAgain(MochiVM* vm, ObjFiber* fiber) {

}