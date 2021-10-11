#include <stdio.h>
#include <string.h>

#include "value.h"
#include "memory.h"

DEFINE_BUFFER(Byte, uint8_t);
DEFINE_BUFFER(Int, int);
DEFINE_BUFFER(Value, Value);

#define ALLOCATE_OBJ(vm, type, objectType) (type*)allocateObject((vm), sizeof(type), (objectType))

static Obj* allocateObject(ZZVM* vm, size_t size, ObjType type) {
    Obj* object = (Obj*)zzReallocate(vm, NULL, 0, size);
    object->type = type;
    // keep track of all allocated objects via the linked list in the vm
    object->next = vm->objects;
    vm->objects = object;
    return object;
}

static ObjString* allocateString(char* chars, int length, ZZVM* vm) {
    ObjString* string = ALLOCATE_OBJ(vm, ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    return string;
}

ObjString* takeString(char* chars, int length, ZZVM* vm) {
    return allocateString(chars, length, vm);
}

ObjString* copyString(const char* chars, int length, ZZVM* vm) {
    char* heapChars = ALLOCATE_ARRAY(vm, char, length + 1);
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';
    return allocateString(heapChars, length, vm);
}

ObjVarFrame* newVarFrame(Value* vars, int varCount, ZZVM* vm) {
    ObjVarFrame* frame = ALLOCATE_OBJ(vm, ObjVarFrame, OBJ_VAR_FRAME);
    frame->slots = vars;
    frame->slotCount = varCount;
    return frame;
}

ObjCallFrame* newCallFrame(Value* vars, int varCount, uint8_t* afterLocation, ZZVM* vm) {
    ObjCallFrame* frame = ALLOCATE_OBJ(vm, ObjCallFrame, OBJ_CALL_FRAME);
    frame->vars.slots = vars;
    frame->vars.slotCount = varCount;
    frame->afterLocation = afterLocation;
    return frame;
}

ObjFiber* zzNewFiber(ZZVM* vm, uint8_t* first, Value* initialStack, int initialStackCount) {
    // Allocate the arrays before the fiber in case it triggers a GC.
    Value* values = ALLOCATE_ARRAY(vm, Value, vm->config.valueStackCapacity);
    ObjVarFrame** frames = ALLOCATE_ARRAY(vm, ObjVarFrame*, vm->config.frameStackCapacity);
    
    ObjFiber* fiber = ALLOCATE_OBJ(vm, ObjFiber, OBJ_FIBER);
    fiber->valueStack = values;
    fiber->frameStack = frames;
    fiber->valueStackTop = values;
    fiber->frameStackTop = frames;

    for (int i = 0; i < initialStackCount; i++) {
        values[i] = initialStack[i];
    }
    fiber->valueStackTop += initialStackCount;

    fiber->isRoot = false;
    fiber->caller = NULL;
    return fiber;
}

ObjCodeBlock* zzNewCodeBlock(ZZVM* vm) {
    ObjCodeBlock* block = ALLOCATE_OBJ(vm, ObjCodeBlock, OBJ_CODE_BLOCK);
    zzValueBufferInit(&block->constants);
    zzByteBufferInit(&block->code);
    zzIntBufferInit(&block->lines);
    return block;
}

ObjForeign* wrenNewForeign(ZZVM* vm, size_t size) {
    ObjForeign* object = ALLOCATE_FLEX(vm, ObjForeign, uint8_t, size);
    object->obj.type = OBJ_FOREIGN;
    object->obj.next = vm->objects;
    vm->objects = (Obj*)object;

    // Zero out the bytes.
    memset(object->data, 0, size);
    return object;
}

static void freeVarFrame(ZZVM* vm, ObjVarFrame* frame) {
    DEALLOCATE(vm, frame->slots);
}

static void freeClosure(ZZVM* vm, ObjClosure* closure) {
    DEALLOCATE(vm, closure->vars);
}

void zzFreeObj(ZZVM* vm, Obj* object) {

#if ZHENZHU_DEBUG_TRACE_MEMORY
    printf("free ");
    printValue(OBJ_VAL(object));
    printf(" @ %p\n", object);
#endif

    switch (object->type) {
        case OBJ_CODE_BLOCK: {
            ObjCodeBlock* block = (ObjCodeBlock*)object;
            zzByteBufferClear(vm, &block->code);
            zzValueBufferClear(vm, &block->constants);
            zzIntBufferClear(vm, &block->lines);
            break;
        }
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
            ObjFiber* fiber = (ObjFiber*)object;
            DEALLOCATE(vm, fiber->valueStack);
            DEALLOCATE(vm, fiber->frameStack);
        }
        case OBJ_FOREIGN: break;
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

void printObject(Value object) {
    switch (AS_OBJ(object)->type) {
        case OBJ_CODE_BLOCK: {
            printf("code");
            break;
        }
        case OBJ_STRING: {
            printf("\"%s\"", AS_CSTRING(object));
            break;
        }
        case OBJ_VAR_FRAME: {
            ObjVarFrame* frame = AS_VAR_FRAME(object);
            printf("frame(%d)", frame->slotCount);
            break;
        }
        case OBJ_CALL_FRAME: {
            ObjCallFrame* frame = AS_CALL_FRAME(object);
            printf("frame(%d -> %p)", frame->vars.slotCount, (void*)frame->afterLocation);
            break;
        }
        case OBJ_MARK_FRAME: {
            ObjMarkFrame* frame = AS_MARK_FRAME(object);
            printf("frame(%d: %d -> %p)", frame->markId, frame->call.vars.slotCount, (void*)frame->call.afterLocation);
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
        case OBJ_FOREIGN: {
            printf("foreign");
            break;
        }
    }
}

void zzGrayObj(ZZVM* vm, Obj* obj) {
    if (obj == NULL) return;

    // Stop if the object is already darkened so we don't get stuck in a cycle.
    if (obj->isMarked) return;

    // It's been reached.
    obj->isMarked = true;

    // Add it to the gray list so it can be recursively explored for
    // more marks later.
    if (vm->grayCount >= vm->grayCapacity) {
        vm->grayCapacity = vm->grayCount * 2;
        vm->gray = (Obj**)vm->config.reallocateFn(vm->gray,
                                                  vm->grayCapacity * sizeof(Obj*),
                                                  vm->config.userData);
    }

    vm->gray[vm->grayCount++] = obj;
}

void zzGrayValue(ZZVM* vm, Value value) {
    if (!IS_OBJ(value)) return;
    zzGrayObj(vm, AS_OBJ(value));
}

void zzGrayBuffer(ZZVM* vm, ValueBuffer* buffer) {
    for (int i = 0; i < buffer->count; i++) {
        zzGrayValue(vm, buffer->data[i]);
    }
}

static void markCodeBlock(ZZVM* vm, ObjCodeBlock* block) {
    zzGrayBuffer(vm, &block->constants);

    vm->bytesAllocated += sizeof(ObjCodeBlock);
    vm->bytesAllocated += sizeof(uint8_t) * block->code.capacity;
    vm->bytesAllocated += sizeof(Value) * block->constants.capacity;
    vm->bytesAllocated += sizeof(int) * block->lines.capacity;
}

static void markVarFrame(ZZVM* vm, ObjVarFrame* frame) {
    for (int i = 0; i < frame->slotCount; i++) {
        zzGrayValue(vm, frame->slots[i]);
    }

    vm->bytesAllocated += sizeof(ObjVarFrame);
    vm->bytesAllocated += sizeof(Value) * frame->slotCount;
}

static void markCallFrame(ZZVM* vm, ObjCallFrame* frame) {
    for (int i = 0; i < frame->vars.slotCount; i++) {
        zzGrayValue(vm, frame->vars.slots[i]);
    }

    vm->bytesAllocated += sizeof(ObjCallFrame);
    vm->bytesAllocated += sizeof(Value) * frame->vars.slotCount;
}

static void markMarkFrame(ZZVM* vm, ObjMarkFrame* frame) {
    for (int i = 0; i < frame->call.vars.slotCount; i++) {
        zzGrayValue(vm, frame->call.vars.slots[i]);
    }

    for (int i = 0; i < frame->operationCount; i++) {
        zzGrayValue(vm, frame->operations[i]);
    }

    vm->bytesAllocated += sizeof(ObjMarkFrame);
    vm->bytesAllocated += sizeof(Value) * frame->call.vars.slotCount;
    vm->bytesAllocated += sizeof(Value) * frame->operationCount;
}

static void markClosure(ZZVM* vm, ObjClosure* closure) {
    for (int i = 0; i < closure->varCount; i++) {
        zzGrayValue(vm, closure->vars[i]);
    }

    vm->bytesAllocated += sizeof(ObjClosure);
    vm->bytesAllocated += sizeof(Value) * closure->varCount;
}

static void markOpClosure(ZZVM* vm, ObjOpClosure* closure) {
    for (int i = 0; i < closure->closure.varCount; i++) {
        zzGrayValue(vm, closure->closure.vars[i]);
    }

    vm->bytesAllocated += sizeof(ObjOpClosure);
    vm->bytesAllocated += sizeof(Value) * closure->closure.varCount;
}

static void markContinuation(ZZVM* vm, ObjContinuation* cont) {
    for (int i = 0; i < cont->savedStackCount; i++) {
        zzGrayValue(vm, cont->savedStack[i]);
    }
    for (int i = 0; i < cont->savedFramesCount; i++) {
        zzGrayObj(vm, (Obj*)cont->savedFrames[i]);
    }

    vm->bytesAllocated += sizeof(ObjContinuation);
    vm->bytesAllocated += sizeof(Value) * cont->savedStackCount;
    vm->bytesAllocated += sizeof(ObjVarFrame*) * cont->savedFramesCount;
}

static void markFiber(ZZVM* vm, ObjFiber* fiber) {
    // Stack variables.
    for (Value* slot = fiber->valueStack; slot < fiber->valueStackTop; slot++) {
        zzGrayValue(vm, *slot);
    }

    // Call stack frames.
    for (ObjVarFrame** slot = fiber->frameStack; slot < fiber->frameStackTop; slot++) {
        zzGrayObj(vm, (Obj*)*slot);
    }

    // The caller.
    zzGrayObj(vm, (Obj*)fiber->caller);

    vm->bytesAllocated += sizeof(ObjFiber);
    vm->bytesAllocated += vm->config.frameStackCapacity * sizeof(ObjVarFrame*);
    vm->bytesAllocated += vm->config.valueStackCapacity * sizeof(Value);
}

static void markString(ZZVM* vm, ObjString* string) {
    vm->bytesAllocated += sizeof(ObjString) + string->length + 1;
}

static void markForeign(ZZVM* vm, ObjForeign* foreign) {
    vm->bytesAllocated += sizeof(Obj) + sizeof(int);
    vm->bytesAllocated += sizeof(uint8_t) * foreign->dataCount;
}

static void blackenObject(ZZVM* vm, Obj* obj)
{
#if ZHEnZHU_DEBUG_TRACE_MEMORY
    printf("mark ");
    printValue(OBJ_VAL(obj));
    printf(" @ %p\n", obj);
#endif

    // Traverse the object's fields.
    switch (obj->type)
    {
        case OBJ_CODE_BLOCK:    markCodeBlock(vm, (ObjCodeBlock*)obj); break;
        case OBJ_VAR_FRAME:     markVarFrame(vm, (ObjVarFrame*)obj); break;
        case OBJ_CALL_FRAME:    markCallFrame(vm, (ObjCallFrame*)obj); break;
        case OBJ_MARK_FRAME:    markMarkFrame(vm, (ObjMarkFrame*)obj); break;
        case OBJ_CLOSURE:       markClosure( vm, (ObjClosure*) obj); break;
        case OBJ_OP_CLOSURE:    markOpClosure(vm, (ObjOpClosure*)obj); break;
        case OBJ_CONTINUATION:  markContinuation(vm, (ObjContinuation*)obj); break;
        case OBJ_FIBER:         markFiber(vm, (ObjFiber*)obj); break;
        case OBJ_STRING:        markString(vm, (ObjString*)obj); break;
        case OBJ_FOREIGN:       markForeign(vm, (ObjForeign*)obj); break;
    }
}

void zzBlackenObjects(ZZVM* vm) {
    while (vm->grayCount > 0) {
        // Pop an item from the gray stack.
        Obj* obj = vm->gray[--vm->grayCount];
        blackenObject(vm, obj);
    }
}