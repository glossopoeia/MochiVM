#include <stdio.h>
#include <string.h>

#include "value.h"
#include "object.h"
#include "memory.h"

void initValueArray(ValueArray * array) {
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}

void writeValueArray(ValueArray* array, Value value) {
    if (array->capacity < array->count + 1) {
        int oldCapacity = array->capacity;
        array->capacity = GROW_CAPACITY(oldCapacity);
        array->values = GROW_ARRAY(Value, array->values, oldCapacity, array->capacity);
    }

    array->values[array->count] = value;
    array->count++;
}

void freeValueArray(ValueArray* array) {
    FREE_ARRAY(Value, array->values, array->capacity);
    initValueArray(array);
}

static void freeVarFrame(ObjVarFrame* frame) {
    FREE_ARRAY(Value, frame->slots, frame->slotCount);
}

static void freeClosure(ObjClosure* closure) {
    FREE_ARRAY(Value, closure->vars, closure->varCount);
}

void freeObject(Obj* object) {
    switch (object->type) {
        case OBJ_STRING: {
            ObjString* string = (ObjString*)object;
            FREE_ARRAY(char, string->chars, string->length + 1);
            FREE(ObjString, object);
            break;
        }
        case OBJ_VAR_FRAME: {
            freeVarFrame((ObjVarFrame*)object);
            FREE(ObjVarFrame, object);
            break;
        }
        case OBJ_CALL_FRAME: {
            freeVarFrame((ObjVarFrame*)object);
            FREE(ObjCallFrame, object);
            break;
        }
        case OBJ_MARK_FRAME: {
            freeVarFrame((ObjVarFrame*)object);
            ObjMarkFrame* mark = (ObjMarkFrame*)object;
            FREE_ARRAY(Value, mark->operations, mark->operationCount);
            FREE(ObjMarkFrame, object);
            break;
        }
        case OBJ_CLOSURE: {
            freeClosure((ObjClosure*)object);
            FREE(ObjClosure, object);
            break;
        }
        case OBJ_OP_CLOSURE: {
            freeClosure((ObjClosure*)object);
            FREE(ObjOpClosure, object);
            break;
        }
        case OBJ_CONTINUATION: {
            ObjContinuation* cont = (ObjContinuation*)object;
            FREE_ARRAY(Value, cont->savedStack, cont->savedStackCount);
            FREE_ARRAY(ObjVarFrame*, cont->savedFrames, cont->savedFramesCount);
            FREE(ObjContinuation, object);
            break;
        }
        case OBJ_FIBER: {
            ASSERT(false, "Freeing fibers currently unimplemented.");
        }
    }
}

void printValue(Value value) {
    if (IS_OBJ(value)) {
        printObject(value);
    } else {
        printf("%f", AS_NUMBER(value));
    }
}