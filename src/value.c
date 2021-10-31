#include <stdio.h>
#include <string.h>

#include "value.h"
#include "memory.h"

DEFINE_BUFFER(Byte, uint8_t);
DEFINE_BUFFER(Int, int);
DEFINE_BUFFER(Value, Value);

#define ALLOCATE_OBJ(vm, type, objectType) (type*)allocateObject((vm), sizeof(type), (objectType))

#define ALLOCATE_FLEX_OBJ(vm, objectType, type, elemType, elemCount)  \
    (type*)allocateObject((vm), sizeof(type) + sizeof(elemType) * elemCount, (objectType))

static void initObj(MochiVM* vm, Obj* obj, ObjType type) {
    obj->type = type;
    obj->isMarked = false;
    // keep track of all allocated objects via the linked list in the vm
    obj->next = vm->objects;
    vm->objects = obj;
}

static Obj* allocateObject(MochiVM* vm, size_t size, ObjType type) {
    Obj* object = (Obj*)mochiReallocate(vm, NULL, 0, size);
    object->type = type;
    object->isMarked = false;
    // keep track of all allocated objects via the linked list in the vm
    object->next = vm->objects;
    vm->objects = object;
    return object;
}

static ObjString* allocateString(char* chars, int length, MochiVM* vm) {
    ObjString* string = ALLOCATE_OBJ(vm, ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    return string;
}

ObjString* takeString(char* chars, int length, MochiVM* vm) {
    return allocateString(chars, length, vm);
}

ObjString* copyString(const char* chars, int length, MochiVM* vm) {
    char* heapChars = ALLOCATE_ARRAY(vm, char, length + 1);
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';
    return allocateString(heapChars, length, vm);
}

ObjVarFrame* newVarFrame(Value* vars, int varCount, MochiVM* vm) {
    ObjVarFrame* frame = ALLOCATE_OBJ(vm, ObjVarFrame, OBJ_VAR_FRAME);
    frame->slots = vars;
    frame->slotCount = varCount;
    return frame;
}

ObjCallFrame* newCallFrame(Value* vars, int varCount, uint8_t* afterLocation, MochiVM* vm) {
    ObjCallFrame* frame = ALLOCATE_OBJ(vm, ObjCallFrame, OBJ_CALL_FRAME);
    frame->vars.slots = vars;
    frame->vars.slotCount = varCount;
    frame->afterLocation = afterLocation;
    return frame;
}

ObjHandleFrame* mochinewHandleFrame(MochiVM* vm, int handleId, uint8_t paramCount, uint8_t handlerCount, uint8_t* after) {
    Value* params = ALLOCATE_ARRAY(vm, Value, paramCount);
    memset(params, 0, sizeof(Value) * paramCount);
    ObjClosure** handlers = ALLOCATE_ARRAY(vm, ObjClosure*, handlerCount);
    memset(handlers, 0, sizeof(ObjClosure*) * handlerCount);

    ObjHandleFrame* frame = ALLOCATE(vm, ObjHandleFrame);
    initObj(vm, (Obj*)frame, OBJ_HANDLE_FRAME);
    frame->call.vars.slots = params;
    frame->call.vars.slotCount = paramCount;
    frame->call.afterLocation = after;
    frame->handleId = handleId;
    frame->nesting = 0;
    frame->afterClosure = NULL;
    frame->handlers = handlers;
    frame->handlerCount = handlerCount;

    return frame;
}

ObjFiber* mochiNewFiber(MochiVM* vm, uint8_t* first, Value* initialStack, int initialStackCount) {
    // Allocate the arrays before the fiber in case it triggers a GC.
    Value* values = ALLOCATE_ARRAY(vm, Value, vm->config.valueStackCapacity);
    ObjVarFrame** frames = ALLOCATE_ARRAY(vm, ObjVarFrame*, vm->config.frameStackCapacity);
    Obj** roots = ALLOCATE_ARRAY(vm, Obj*, vm->config.rootStackCapacity);
    
    ObjFiber* fiber = ALLOCATE_OBJ(vm, ObjFiber, OBJ_FIBER);
    fiber->valueStack = values;
    fiber->valueStackTop = values;
    fiber->frameStack = frames;
    fiber->frameStackTop = frames;
    fiber->rootStack = roots;
    fiber->rootStackTop = roots;

    for (int i = 0; i < initialStackCount; i++) {
        values[i] = initialStack[i];
    }
    fiber->valueStackTop += initialStackCount;

    fiber->isRoot = false;
    fiber->caller = NULL;
    return fiber;
}

void mochiFiberPushValue(ObjFiber* fiber, Value v) {
    *fiber->valueStackTop++ = v;
}

Value mochiFiberPopValue(ObjFiber* fiber) {
    return *(--fiber->valueStackTop);
}

void mochiFiberPushRoot(ObjFiber* fiber, Obj* root) {
    *fiber->rootStackTop++ = root;
}

void mochiFiberPopRoot(ObjFiber* fiber) {
    fiber->rootStackTop--;
}

ObjClosure* mochiNewClosure(MochiVM* vm, uint8_t* body, uint8_t paramCount, uint16_t capturedCount) {
    ObjClosure* closure = ALLOCATE_FLEX(vm, ObjClosure, Value, capturedCount);
    initObj(vm, (Obj*)closure, OBJ_CLOSURE);
    closure->funcLocation = body;
    closure->paramCount = paramCount;
    closure->capturedCount = capturedCount;
    // reset the captured array in case there is a GC in between allocating and populating it
    memset(closure->captured, 0, sizeof(Value) * capturedCount);
    return closure;
}

void mochiClosureCapture(ObjClosure* closure, int captureIndex, Value value) {
    ASSERT(captureIndex < closure->capturedCount, "Closure capture index outside the bounds of the captured array");
    closure->captured[captureIndex] = value;
}

ObjContinuation* mochiNewContinuation(MochiVM* vm, uint8_t* resume, uint8_t paramCount, int savedStackCount, int savedFramesCount) {
    Value* savedStack = ALLOCATE_ARRAY(vm, Value, savedStackCount);
    memset(savedStack, 0, sizeof(Value) * savedStackCount);
    ObjVarFrame** savedFrames = ALLOCATE_ARRAY(vm, ObjVarFrame*, savedFramesCount);
    memset(savedFrames, 0, sizeof(ObjVarFrame*) * savedFramesCount);

    ObjContinuation* cont = ALLOCATE(vm, ObjContinuation);
    initObj(vm, (Obj*)cont, OBJ_CONTINUATION);
    cont->resumeLocation = resume;
    cont->paramCount = paramCount;
    cont->savedStack = savedStack;
    cont->savedFrames = savedFrames;
    cont->savedStackCount = savedStackCount;
    cont->savedFramesCount = savedFramesCount;
    return cont;
}

ObjCodeBlock* mochiNewCodeBlock(MochiVM* vm) {
    ObjCodeBlock* block = ALLOCATE(vm, ObjCodeBlock);
    initObj(vm, (Obj*)block, OBJ_CODE_BLOCK);
    mochiValueBufferInit(&block->constants);
    mochiByteBufferInit(&block->code);
    mochiIntBufferInit(&block->lines);
    return block;
}

ObjForeign* mochiNewForeign(MochiVM* vm, size_t size) {
    ObjForeign* object = ALLOCATE_FLEX(vm, ObjForeign, uint8_t, size);
    initObj(vm, (Obj*)object, OBJ_FOREIGN);

    // Zero out the bytes.
    memset(object->data, 0, size);
    return object;
}

ObjCPointer* mochiNewCPointer(MochiVM* vm, void* pointer) {
    ObjCPointer* object = ALLOCATE_OBJ(vm, ObjCPointer, OBJ_C_POINTER);
    object->pointer = pointer;
    return object;
}

ObjList* mochiListNil(MochiVM* vm) {
    return NULL;
}

ObjList* mochiListCons(MochiVM* vm, Value elem, ObjList* tail) {
    ObjList* list = ALLOCATE(vm, ObjList);
    initObj(vm, (Obj*)list, OBJ_LIST);
    list->next = tail;
    list->elem = elem;
    return list;
}

ObjList* mochiListTail(ObjList* list) {
    return list->next;
}

Value mochiListHead(ObjList* list) {
    return list->elem;
}

int mochiListLength(ObjList* list) {
    int count = 0;
    while (list != NULL) {
        list = list->next;
        count++;
    }
    return count;
}

static void freeVarFrame(MochiVM* vm, ObjVarFrame* frame) {
    DEALLOCATE(vm, frame->slots);
}

void mochiFreeObj(MochiVM* vm, Obj* object) {

#if MOCHIVM_DEBUG_TRACE_MEMORY
    printf("free ");
    printValue(vm, OBJ_VAL(object));
    printf(" @ %p\n", object);
#endif

    switch (object->type) {
        case OBJ_CODE_BLOCK: {
            ObjCodeBlock* block = (ObjCodeBlock*)object;
            mochiByteBufferClear(vm, &block->code);
            mochiValueBufferClear(vm, &block->constants);
            mochiIntBufferClear(vm, &block->lines);
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
        case OBJ_HANDLE_FRAME: {
            freeVarFrame(vm, (ObjVarFrame*)object);
            ObjHandleFrame* handle = (ObjHandleFrame*)object;
            DEALLOCATE(vm, handle->handlers);
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
            DEALLOCATE(vm, fiber->rootStack);
        }
        case OBJ_CLOSURE: break;
        case OBJ_FOREIGN: break;
        case OBJ_C_POINTER: break;
        case OBJ_LIST: break;
    }

    DEALLOCATE(vm, object);
}

void printValue(MochiVM* vm, Value value) {
    if (IS_OBJ(value)) {
        printObject(vm, value);
    } else {
        printf("%f", AS_NUMBER(value));
    }
}

void printObject(MochiVM* vm, Value object) {
    if (AS_OBJ(object) == NULL) {
        printf("nil");
        return;
    }

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
            printf("var(%d)", frame->slotCount);
            break;
        }
        case OBJ_CALL_FRAME: {
            ObjCallFrame* frame = AS_CALL_FRAME(object);
            printf("call(%d -> %ld)", frame->vars.slotCount, frame->afterLocation - vm->block->code.data);
            break;
        }
        case OBJ_HANDLE_FRAME: {
            ObjHandleFrame* frame = AS_HANDLE_FRAME(object);
            printf("handle(%d: n(%d) %d %d -> %ld)", frame->handleId, frame->nesting, frame->handlerCount,
                    frame->call.vars.slotCount, frame->call.afterLocation - vm->block->code.data);
            break;
        }
        case OBJ_CLOSURE: {
            ObjClosure* closure = AS_CLOSURE(object);
            printf("closure(%d: %d -> %ld)", closure->capturedCount, closure->paramCount, closure->funcLocation - vm->block->code.data);
            break;
        }
        case OBJ_CONTINUATION: {
            ObjContinuation* cont = AS_CONTINUATION(object);
            printf("continuation(%d: v(%d) f(%d) -> %ld)", cont->paramCount, cont->savedStackCount, cont->savedFramesCount, cont->resumeLocation - vm->block->code.data);
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
        case OBJ_C_POINTER: {
            printf("c_ptr");
            break;
        }
        case OBJ_LIST: {
            ObjList* list = AS_LIST(object);
            printf("cons(");
            printValue(vm, list->elem);
            printf(",");
            printObject(vm, OBJ_VAL(list->next));
            printf(")");
            break;
        }
    }
}

void mochiGrayObj(MochiVM* vm, Obj* obj) {
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

void mochiGrayValue(MochiVM* vm, Value value) {
    if (!IS_OBJ(value)) return;
    mochiGrayObj(vm, AS_OBJ(value));
}

void mochiGrayBuffer(MochiVM* vm, ValueBuffer* buffer) {
    for (int i = 0; i < buffer->count; i++) {
        mochiGrayValue(vm, buffer->data[i]);
    }
}

static void markCodeBlock(MochiVM* vm, ObjCodeBlock* block) {
    mochiGrayBuffer(vm, &block->constants);

    vm->bytesAllocated += sizeof(ObjCodeBlock);
    vm->bytesAllocated += sizeof(uint8_t) * block->code.capacity;
    vm->bytesAllocated += sizeof(Value) * block->constants.capacity;
    vm->bytesAllocated += sizeof(int) * block->lines.capacity;
}

static void markVarFrame(MochiVM* vm, ObjVarFrame* frame) {
    for (int i = 0; i < frame->slotCount; i++) {
        mochiGrayValue(vm, frame->slots[i]);
    }

    vm->bytesAllocated += sizeof(ObjVarFrame);
    vm->bytesAllocated += sizeof(Value) * frame->slotCount;
}

static void markCallFrame(MochiVM* vm, ObjCallFrame* frame) {
    for (int i = 0; i < frame->vars.slotCount; i++) {
        mochiGrayValue(vm, frame->vars.slots[i]);
    }

    vm->bytesAllocated += sizeof(ObjCallFrame);
    vm->bytesAllocated += sizeof(Value) * frame->vars.slotCount;
}

static void markHandleFrame(MochiVM* vm, ObjHandleFrame* frame) {
    for (int i = 0; i < frame->call.vars.slotCount; i++) {
        mochiGrayValue(vm, frame->call.vars.slots[i]);
    }

    mochiGrayObj(vm, (Obj*)frame->afterClosure);
    for (int i = 0; i < frame->handlerCount; i++) {
        mochiGrayObj(vm, (Obj*)frame->handlers[i]);
    }

    vm->bytesAllocated += sizeof(ObjHandleFrame);
    vm->bytesAllocated += sizeof(Value) * frame->call.vars.slotCount;
    vm->bytesAllocated += sizeof(ObjClosure*) * frame->handlerCount;
}

static void markClosure(MochiVM* vm, ObjClosure* closure) {
    for (int i = 0; i < closure->capturedCount; i++) {
        mochiGrayValue(vm, closure->captured[i]);
    }

    vm->bytesAllocated += sizeof(ObjClosure);
    vm->bytesAllocated += sizeof(Value) * closure->capturedCount;
}

static void markContinuation(MochiVM* vm, ObjContinuation* cont) {
    for (int i = 0; i < cont->savedStackCount; i++) {
        mochiGrayValue(vm, cont->savedStack[i]);
    }
    for (int i = 0; i < cont->savedFramesCount; i++) {
        mochiGrayObj(vm, (Obj*)cont->savedFrames[i]);
    }

    vm->bytesAllocated += sizeof(ObjContinuation);
    vm->bytesAllocated += sizeof(Value) * cont->savedStackCount;
    vm->bytesAllocated += sizeof(ObjVarFrame*) * cont->savedFramesCount;
}

static void markFiber(MochiVM* vm, ObjFiber* fiber) {
    // Stack variables.
    for (Value* slot = fiber->valueStack; slot < fiber->valueStackTop; slot++) {
        mochiGrayValue(vm, *slot);
    }

    // Call stack frames.
    for (ObjVarFrame** slot = fiber->frameStack; slot < fiber->frameStackTop; slot++) {
        mochiGrayObj(vm, (Obj*)*slot);
    }

    // Root stack.
    for (Obj** slot = fiber->rootStack; slot < fiber->rootStackTop; slot++) {
        mochiGrayObj(vm, *slot);
    }

    // The caller.
    mochiGrayObj(vm, (Obj*)fiber->caller);

    vm->bytesAllocated += sizeof(ObjFiber);
    vm->bytesAllocated += vm->config.frameStackCapacity * sizeof(ObjVarFrame*);
    vm->bytesAllocated += vm->config.valueStackCapacity * sizeof(Value);
    vm->bytesAllocated += vm->config.rootStackCapacity * sizeof(Obj*);
}

static void markString(MochiVM* vm, ObjString* string) {
    vm->bytesAllocated += sizeof(ObjString) + string->length + 1;
}

static void markForeign(MochiVM* vm, ObjForeign* foreign) {
    vm->bytesAllocated += sizeof(Obj) + sizeof(int);
    vm->bytesAllocated += sizeof(uint8_t) * foreign->dataCount;
}

static void markCPointer(MochiVM* vm, ObjCPointer* ptr) {
    vm->bytesAllocated += sizeof(ObjCPointer);
}

static void markList(MochiVM* vm, ObjList* list) {
    mochiGrayValue(vm, list->elem);
    mochiGrayObj(vm, (Obj*)list->next);

    vm->bytesAllocated += sizeof(ObjList);
}

static void blackenObject(MochiVM* vm, Obj* obj)
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
        case OBJ_HANDLE_FRAME:  markHandleFrame(vm, (ObjHandleFrame*)obj); break;
        case OBJ_CLOSURE:       markClosure( vm, (ObjClosure*) obj); break;
        case OBJ_CONTINUATION:  markContinuation(vm, (ObjContinuation*)obj); break;
        case OBJ_FIBER:         markFiber(vm, (ObjFiber*)obj); break;
        case OBJ_STRING:        markString(vm, (ObjString*)obj); break;
        case OBJ_FOREIGN:       markForeign(vm, (ObjForeign*)obj); break;
        case OBJ_C_POINTER:     markCPointer(vm, (ObjCPointer*)obj); break;
        case OBJ_LIST:          markList(vm, (ObjList*)obj); break;
    }
}

void mochiBlackenObjects(MochiVM* vm) {
    while (vm->grayCount > 0) {
        // Pop an item from the gray stack.
        Obj* obj = vm->gray[--vm->grayCount];
        blackenObject(vm, obj);
    }
}