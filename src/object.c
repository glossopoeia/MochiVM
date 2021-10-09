#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

void printObject(Value object) {
    switch (AS_OBJ(object)->type) {
        case OBJ_STRING:
            printf("%s", AS_CSTRING(object));
            break;
        case OBJ_VAR_FRAME: {
            ObjVarFrame* frame = AS_VAR_FRAME(object);
            printf("%d", frame->slotCount);
            break;
        }
        case OBJ_CALL_FRAME: {
            ObjCallFrame* frame = AS_CALL_FRAME(object);
            printf("%d -> %p", frame->vars.slotCount, (void*)frame->afterLocation);
            break;
        }
        case OBJ_MARK_FRAME: {
            ObjMarkFrame* frame = AS_MARK_FRAME(object);
            printf("%d: %d -> %p", frame->markId, frame->call.vars.slotCount, (void*)frame->call.afterLocation);
            break;
        }
        case OBJ_CLOSURE: {
            printf("closure");
            break;
        }
        case OBJ_OP_CLOSURE: {
            printf("op-closure");
            break;
        }
        case OBJ_CONTINUATION: {
            printf("continuation");
            break;
        }
        case OBJ_FIBER: {
            printf("fiber");
            break;
        }
    }
}