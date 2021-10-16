#include "battery_uv.h"
#include "vm.h"
#include "uv.h"

void uvzzNewTimer(ZZVM* vm, ObjFiber* fiber) {
    uv_timer_t* timer = vm->config.reallocateFn(NULL, sizeof(uv_timer_t*), vm->config.userData);
    uv_timer_init(uv_default_loop(), timer);

    ObjCPointer* ptr = zzNewCPointer(vm, timer);
    zzFiberPushValue(fiber, OBJ_VAL(ptr));
}

void uvzzCloseTimer(ZZVM* vm, ObjFiber* fiber) {
    ObjCPointer* ptr = (ObjCPointer*)AS_OBJ(zzFiberPopValue(fiber));
    uv_timer_stop((uv_timer_t*)ptr->pointer);
    vm->config.reallocateFn(ptr->pointer, 0, vm->config.userData);
}

void uvzzTimerStart(ZZVM* vm, ObjFiber* fiber) {
    
}

void uvzzTimerStop(ZZVM* vm, ObjFiber* fiber) {

}

void uvzzTimerSetRepeat(ZZVM* vm, ObjFiber* fiber) {

}

void uvzzTimerAgain(ZZVM* vm, ObjFiber* fiber) {

}