#include <stdio.h>
#include <string.h>

#include "value.h"
#include "object.h"
#include "memory.h"

DEFINE_BUFFER(Value, Value);
DEFINE_BUFFER(Byte, uint8_t);
DEFINE_BUFFER(Int, int);

static void freeVarFrame(ZZVM* vm, ObjVarFrame* frame) {
    DEALLOCATE(vm, frame->slots);
}

static void freeClosure(ZZVM* vm, ObjClosure* closure) {
    DEALLOCATE(vm, closure->vars);
}

void zzFreeObj(ZZVM* vm, Obj* object) {
    switch (object->type) {
        case OBJ_STRING: {
            ObjString* string = (ObjString*)object;
            DEALLOCATE(vm, string->chars);
            break;
        }
        case OBJ_VAR_FRAME: {
            freeVarFrame(vm, (ObjVarFrame*)object);
            break;
        }
        case OBJ_CALL_FRAME: {
            freeVarFrame(vm, (ObjVarFrame*)object);
            break;
        }
        case OBJ_MARK_FRAME: {
            freeVarFrame(vm, (ObjVarFrame*)object);
            ObjMarkFrame* mark = (ObjMarkFrame*)object;
            DEALLOCATE(vm, mark->operations);
            break;
        }
        case OBJ_CLOSURE: {
            freeClosure(vm, (ObjClosure*)object);
            break;
        }
        case OBJ_OP_CLOSURE: {
            freeClosure(vm, (ObjClosure*)object);
            break;
        }
        case OBJ_CONTINUATION: {
            ObjContinuation* cont = (ObjContinuation*)object;
            DEALLOCATE(vm, cont->savedStack);
            DEALLOCATE(vm, cont->savedFrames);
            break;
        }
        case OBJ_FIBER: {
            ASSERT(false, "Freeing fibers currently unimplemented.");
        }
    }

    DEALLOCATE(vm, object);
}

void printValue(Value value) {
    if (IS_OBJ(value)) {
        printObject(value);
    } else {
        printf("%f", AS_NUMBER(value));
    }
}