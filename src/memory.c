#include <stdlib.h>

#include "memory.h"

void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
    if (newSize == 0) {
        free(pointer);
        return NULL;
    }

    void* result = realloc(pointer, newSize);
    if (result == NULL) { exit(1); }
    return result;
}

void freeObjects(VM* vm) {
    Obj* object = vm->objects;
    while (object != NULL) {
        Obj* next = object->next;
        freeObject(object);
        object = next;
    }
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
    }
}