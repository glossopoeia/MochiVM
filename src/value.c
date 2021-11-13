#include <stdio.h>
#include <string.h>

#include "value.h"
#include "memory.h"

DEFINE_BUFFER(Byte, uint8_t);
DEFINE_BUFFER(Int, int);
DEFINE_BUFFER(Value, Value);

static void initObj(MochiVM* vm, Obj* obj, ObjType type) {
    obj->type = type;
    obj->isMarked = false;
    // keep track of all allocated objects via the linked list in the vm
    obj->next = vm->objects;
    vm->objects = obj;
}

ObjI64* mochiNewI64(MochiVM* vm, int64_t val) {
    ObjI64* i = ALLOCATE(vm, ObjI64);
    initObj(vm, (Obj*)i, OBJ_I64);
    i->val = val;
    return i;
}

ObjU64* mochiNewU64(MochiVM* vm, uint64_t val) {
    ObjU64* i = ALLOCATE(vm, ObjU64);
    initObj(vm, (Obj*)i, OBJ_U64);
    i->val = val;
    return i;
}

ObjDouble* mochiNewDouble(MochiVM* vm, double val) {
    ObjDouble* i = ALLOCATE(vm, ObjDouble);
    initObj(vm, (Obj*)i, OBJ_DOUBLE);
    i->val = val;
    return i;
}

static ObjString* allocateString(char* chars, int length, MochiVM* vm) {
    ObjString* string = ALLOCATE(vm, ObjString);
    initObj(vm, (Obj*)string, OBJ_STRING);
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
    ObjVarFrame* frame = ALLOCATE(vm, ObjVarFrame);
    initObj(vm, (Obj*)frame, OBJ_VAR_FRAME);
    frame->slots = vars;
    frame->slotCount = varCount;
    return frame;
}

ObjCallFrame* newCallFrame(Value* vars, int varCount, uint8_t* afterLocation, MochiVM* vm) {
    ObjCallFrame* frame = ALLOCATE(vm, ObjCallFrame);
    initObj(vm, (Obj*)frame, OBJ_CALL_FRAME);
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
    
    ObjFiber* fiber = ALLOCATE(vm, ObjFiber);
    initObj(vm, (Obj*)fiber, OBJ_FIBER);
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

    fiber->isSuspended = false;
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

void mochiFiberPushFrame(ObjFiber* fiber, ObjVarFrame* frame) {
    *fiber->frameStackTop++ = frame;
}

ObjVarFrame* mochiFiberPopFrame(ObjFiber* fiber) {
    return *(--fiber->frameStackTop);
}

void mochiFiberPushRoot(ObjFiber* fiber, Obj* root) {
    *fiber->rootStackTop++ = root;
}

Obj* mochiFiberPopRoot(ObjFiber* fiber) {
    return *(--fiber->rootStackTop);
}

ObjClosure* mochiNewClosure(MochiVM* vm, uint8_t* body, uint8_t paramCount, uint16_t capturedCount) {
    ObjClosure* closure = ALLOCATE_FLEX(vm, ObjClosure, Value, capturedCount);
    initObj(vm, (Obj*)closure, OBJ_CLOSURE);
    closure->funcLocation = body;
    closure->paramCount = paramCount;
    closure->capturedCount = capturedCount;
    // Resume many is the default because multiple resumptions are the most general. Most closures
    // will not actually contain/perform continuation saving or restoring, this default is simply
    // provided so that the safest before for handler closures is assumed by default. Closures which
    // are not used as handlers will ignore this field anyway.
    closure->resumeLimit = RESUME_MANY;
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
    mochiIntBufferInit(&block->labelIndices);
    mochiValueBufferInit(&block->labels);
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
    ObjCPointer* ptr = ALLOCATE(vm, ObjCPointer);
    initObj(vm, (Obj*)ptr, OBJ_C_POINTER);
    ptr->pointer = pointer;
    return ptr;
}

ForeignResume* mochiNewResume(MochiVM* vm, ObjFiber* fiber) {
    ForeignResume* res = ALLOCATE(vm, ForeignResume);
    initObj(vm, (Obj*)res, OBJ_FOREIGN_RESUME);
    res->vm = vm;
    res->fiber = fiber;
    return res;
}

ObjRef* mochiNewRef(MochiVM* vm, HeapKey ptr) {
    ObjRef* ref = ALLOCATE(vm, ObjRef);
    initObj(vm, (Obj*)ref, OBJ_REF);
    ref->ptr = ptr;
    return ref;
}

ObjStruct* mochiNewStruct(MochiVM* vm, StructId id, int elemCount) {
    ObjStruct* stru = ALLOCATE_FLEX(vm, ObjStruct, Value, elemCount);
    initObj(vm, (Obj*)stru, OBJ_STRUCT);
    stru->id = id;
    stru->count = elemCount;
    return stru;
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

ObjArray* mochiArrayNil(MochiVM* vm) {
    ObjArray* arr = ALLOCATE(vm, ObjArray);
    initObj(vm, (Obj*)arr, OBJ_ARRAY);
    mochiValueBufferInit(&arr->elems);
    return arr;
}

ObjArray* mochiArrayFill(MochiVM* vm, int amount, Value elem, ObjArray* array) {
    mochiValueBufferFill(vm, &array->elems, elem, amount);
    return array;
}

ObjArray* mochiArraySnoc(MochiVM* vm, Value elem, ObjArray* array) {
    mochiValueBufferWrite(vm, &array->elems, elem);
    return array;
}

Value mochiArrayGetAt(int index, ObjArray* array) {
    ASSERT(array->elems.count > index, "Tried to access an element beyond the bounds of the Array.");
    return array->elems.data[index];
}

void mochiArraySetAt(int index, Value value, ObjArray* array) {
    ASSERT(array->elems.count > index, "Tried to modify an element beyond the bounds of the Array.");
    array->elems.data[index] = value;
}

int mochiArrayLength(ObjArray* array) {
    return array->elems.count;
}

ObjArray* mochiArrayCopy(MochiVM* vm, int start, int length, ObjArray* array) {
    ObjArray* copy = mochiArrayNil(vm);
    mochiFiberPushRoot(vm->fiber, (Obj*)copy);
    for (int i = 0; i < array->elems.count; i++) {
        mochiValueBufferWrite(vm, &copy->elems, array->elems.data[i]);
    }
    mochiFiberPopRoot(vm->fiber);
    return copy;
}

ObjSlice* mochiArraySlice(MochiVM* vm, int start, int length, ObjArray* array) {
    ASSERT(start + length <= array->elems.count, "Tried to creat a Slice that accesses elements beyond the length of the source Array.");
    ObjSlice* slice = ALLOCATE(vm, ObjSlice);
    initObj(vm, (Obj*)slice, OBJ_SLICE);
    slice->start = start;
    slice->count = length;
    slice->source = array;
    return slice;
}

ObjSlice* mochiSubslice(MochiVM* vm, int start, int length, ObjSlice* slice) {
    return mochiArraySlice(vm, start + slice->start, length, slice->source);
}

Value mochiSliceGetAt(int index, ObjSlice* slice) {
    ASSERT(slice->count > index, "Tried to access an element beyond the bounds of the Slice.");
    return slice->source->elems.data[slice->start + index];
}

void mochiSliceSetAt(int index, Value value, ObjSlice* slice) {
    ASSERT(slice->count > index, "Tried to modify an element beyond the bounds of the Slice.");
    slice->source->elems.data[slice->start + index] = value;
}

int mochiSliceLength(ObjSlice* slice) {
    return slice->count;
}

ObjArray* mochiSliceCopy(MochiVM* vm, ObjSlice* slice) {
    ObjArray* copy = mochiArrayNil(vm);
    mochiFiberPushRoot(vm->fiber, (Obj*)copy);
    for (int i = 0; i < slice->count; i++) {
        mochiValueBufferWrite(vm, &copy->elems, slice->source->elems.data[i]);
    }
    mochiFiberPopRoot(vm->fiber);
    return copy;
}




ObjByteArray* mochiByteArrayNil(MochiVM* vm) {
    ObjByteArray* arr = ALLOCATE(vm, ObjByteArray);
    initObj(vm, (Obj*)arr, OBJ_BYTE_ARRAY);
    mochiByteBufferInit(&arr->elems);
    return arr;
}

ObjByteArray* mochiByteArrayFill(MochiVM* vm, int amount, uint8_t elem, ObjByteArray* array) {
    mochiByteBufferFill(vm, &array->elems, elem, amount);
    return array;
}

ObjByteArray* mochiByteArraySnoc(MochiVM* vm, uint8_t elem, ObjByteArray* array) {
    mochiByteBufferWrite(vm, &array->elems, elem);
    return array;
}

uint8_t mochiByteArrayGetAt(int index, ObjByteArray* array) {
    ASSERT(array->elems.count > index, "Tried to access an element beyond the bounds of the Array.");
    return array->elems.data[index];
}

void mochiByteArraySetAt(int index, uint8_t value, ObjByteArray* array) {
    ASSERT(array->elems.count > index, "Tried to modify an element beyond the bounds of the Array.");
    array->elems.data[index] = value;
}

int mochiByteArrayLength(ObjByteArray* array) {
    return array->elems.count;
}

ObjByteArray* mochiByteArrayCopy(MochiVM* vm, int start, int length, ObjByteArray* array) {
    ObjByteArray* copy = mochiByteArrayNil(vm);
    mochiFiberPushRoot(vm->fiber, (Obj*)copy);
    for (int i = 0; i < array->elems.count; i++) {
        mochiByteBufferWrite(vm, &copy->elems, array->elems.data[i]);
    }
    mochiFiberPopRoot(vm->fiber);
    return copy;
}

ObjByteSlice* mochiByteArraySlice(MochiVM* vm, int start, int length, ObjByteArray* array) {
    ASSERT(start + length <= array->elems.count, "Tried to creat a Slice that accesses elements beyond the length of the source Array.");
    ObjByteSlice* slice = ALLOCATE(vm, ObjByteSlice);
    initObj(vm, (Obj*)slice, OBJ_BYTE_SLICE);
    slice->start = start;
    slice->count = length;
    slice->source = array;
    return slice;
}

ObjByteSlice* mochiByteSubslice(MochiVM* vm, int start, int length, ObjByteSlice* slice) {
    return mochiByteArraySlice(vm, start + slice->start, length, slice->source);
}

uint8_t mochiByteSliceGetAt(int index, ObjByteSlice* slice) {
    ASSERT(slice->count > index, "Tried to access an element beyond the bounds of the Slice.");
    return slice->source->elems.data[slice->start + index];
}

void mochiByteSliceSetAt(int index, uint8_t value, ObjByteSlice* slice) {
    ASSERT(slice->count > index, "Tried to modify an element beyond the bounds of the Slice.");
    slice->source->elems.data[slice->start + index] = value;
}

int mochiByteSliceLength(ObjByteSlice* slice) {
    return slice->count;
}

ObjByteArray* mochiByteSliceCopy(MochiVM* vm, ObjByteSlice* slice) {
    ObjByteArray* copy = mochiByteArrayNil(vm);
    mochiFiberPushRoot(vm->fiber, (Obj*)copy);
    for (int i = 0; i < slice->count; i++) {
        mochiByteBufferWrite(vm, &copy->elems, slice->source->elems.data[i]);
    }
    mochiFiberPopRoot(vm->fiber);
    return copy;
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
            mochiIntBufferClear(vm, &block->labelIndices);
            mochiValueBufferClear(vm, &block->labels);
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
            break;
        }
        case OBJ_ARRAY: {
            ObjArray* arr = (ObjArray*)object;
            mochiValueBufferClear(vm, &arr->elems);
            break;
        }
        case OBJ_BYTE_ARRAY: {
            ObjByteArray* arr = (ObjByteArray*)object;
            mochiByteBufferClear(vm, &arr->elems);
            break;
        }
        case OBJ_REF: {
            ObjRef* ref = (ObjRef*)object;
            // NOTE: this assumes garbage collection, i.e. there are
            // no other ObjRef instances that contain the same ptr value
            // that ref contains. If the ref can be fully copied into
            // a new ObjRef with the same ptr value, the following is
            // no longer valid. But note that this is not the same as
            // having multiple pointers to ref on the value/frame stack,
            // which is perfectly fine since the heap-clearing logic will
            // only occur when ref becomes fully unreachable.
            bool removed = mochiHeapTryRemove(vm, &vm->heap, ref->ptr);
            ASSERT(removed, "Could not clear a reference from the heap.");
            break;
        }
        case OBJ_CLOSURE: break;
        case OBJ_FOREIGN: break;
        case OBJ_C_POINTER: break;
        case OBJ_LIST: break;
        case OBJ_FOREIGN_RESUME: break;
        case OBJ_SLICE: break;
        case OBJ_BYTE_SLICE: break;
        case OBJ_STRUCT: break;
        case OBJ_I64: break;
        case OBJ_U64: break;
        case OBJ_DOUBLE: break;
    }

    DEALLOCATE(vm, object);
}

void printValue(MochiVM* vm, Value value) {
    if (IS_OBJ(value)) {
        printObject(vm, value);
    } else {
#if MOCHIVM_NAN_TAGGING
        printf("%f", AS_DOUBLE(value));
#else
        printf("%ju", value);
#endif
    }
}

void printObject(MochiVM* vm, Value object) {
    if (AS_OBJ(object) == NULL) {
        printf("nil");
        return;
    }

    switch (AS_OBJ(object)->type) {
        case OBJ_I64: {
            ObjI64* i = (ObjI64*)AS_OBJ(object);
            printf("%jd", i->val);
            break;
        }
        case OBJ_U64: {
            ObjU64* i = (ObjU64*)AS_OBJ(object);
            printf("%ju", i->val);
            break;
        }
        case OBJ_DOUBLE: {
            ObjDouble* i = (ObjDouble*)AS_OBJ(object);
            printf("%f", i->val);
            break;
        }
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
        case OBJ_ARRAY: {
            ObjArray* arr = AS_ARRAY(object);
            printf("arr(");
            for (int i = 0; i < arr->elems.count; i++) {
                printValue(vm, arr->elems.data[i]);
                if (i < arr->elems.count - 1) {
                    printf(",");
                }
            }
            printf(")");
            break;
        }
        case OBJ_SLICE: {
            ObjSlice* slice = AS_SLICE(object);
            printf("slice(");
            for (int i = 0; i < slice->count; i++) {
                printValue(vm, slice->source->elems.data[slice->start + i]);
                if (i < slice->count - 1) {
                    printf(",");
                }
            }
            printf(")");
            break;
        }
        case OBJ_BYTE_ARRAY: {
            ObjByteArray* arr = AS_BYTE_ARRAY(object);
            printf("barray(");
            for (int i = 0; i < arr->elems.count; i++) {
                printf("%d", arr->elems.data[i]);
                if (i < arr->elems.count - 1) {
                    printf(",");
                }
            }
            printf(")");
            break;
        }
        case OBJ_BYTE_SLICE: {
            ObjByteSlice* slice = AS_BYTE_SLICE(object);
            printf("bslice(");
            for (int i = 0; i < slice->count; i++) {
                printf("%d", slice->source->elems.data[slice->start + i]);
                if (i < slice->count - 1) {
                    printf(",");
                }
            }
            printf(")");
            break;
        }
        case OBJ_FOREIGN_RESUME: {
            printf("foreign_resume");
            break;
        }
        case OBJ_REF: {
            printf("ref(");
            ObjRef* ref = AS_REF(object);
            Value val;
            if (mochiHeapGet(&vm->heap, ref->ptr, &val)) {
                printValue(vm, val);
            } else {
                printf("NOT_FOUND");
            }
            printf(")");
            break;
        }
        case OBJ_STRUCT: {
            printf("struct(");
            ObjStruct* stru = AS_STRUCT(object);
            for (int i = 0; i < stru->count; i++) {
                printValue(vm, stru->elems[i]);
                if (i < stru->count - 1) {
                    printf(",");
                }
            }
            printf(")");
            break;
        }
        default: {
            printf("unknown(%d)", AS_OBJ(object)->type);
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

#define MARK_SIMPLE(vm, type)   ((vm)->bytesAllocated += sizeof(type))

static void markCodeBlock(MochiVM* vm, ObjCodeBlock* block) {
    mochiGrayBuffer(vm, &block->constants);
    mochiGrayBuffer(vm, &block->labels);

    vm->bytesAllocated += sizeof(ObjCodeBlock);
    vm->bytesAllocated += sizeof(uint8_t) * block->code.capacity;
    vm->bytesAllocated += sizeof(Value) * block->constants.capacity;
    vm->bytesAllocated += sizeof(int) * block->lines.capacity;
    vm->bytesAllocated += sizeof(int) * block->labelIndices.capacity;
    vm->bytesAllocated += sizeof(Value) * block->labels.capacity;
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

static void markList(MochiVM* vm, ObjList* list) {
    mochiGrayValue(vm, list->elem);
    mochiGrayObj(vm, (Obj*)list->next);

    vm->bytesAllocated += sizeof(ObjList);
}

static void markArray(MochiVM* vm, ObjArray* arr) {
    mochiGrayBuffer(vm, &arr->elems);

    vm->bytesAllocated += sizeof(ObjArray);
}

static void markSlice(MochiVM* vm, ObjSlice* slice) {
    mochiGrayObj(vm, (Obj*)slice->source);

    vm->bytesAllocated += sizeof(ObjSlice);
}

static void markByteSlice(MochiVM* vm, ObjByteSlice* slice) {
    mochiGrayObj(vm, (Obj*)slice->source);

    vm->bytesAllocated += sizeof(ObjByteSlice);
}

static void markRef(MochiVM* vm, ObjRef* ref) {
    // TODO: investigate iterating over the heap itself to gray set values, determine if performance benefit/degradation
    Value val;
    if (mochiHeapGet(&vm->heap, ref->ptr, &val)) {
        mochiGrayValue(vm, val);
    } else {
        ASSERT(false, "Ref does not point to a heap slot.");
    }

    vm->bytesAllocated += sizeof(ObjRef);
}

static void markStruct(MochiVM* vm, ObjStruct* stru) {
    for (int i = 0; i < stru->count; i++) {
        mochiGrayValue(vm, stru->elems[i]);
    }

    vm->bytesAllocated += sizeof(ObjStruct) + stru->count * sizeof(Value);
}

static void markForeignResume(MochiVM* vm, ForeignResume* resume) {
    mochiGrayObj(vm, (Obj*)resume->fiber);

    vm->bytesAllocated += sizeof(ForeignResume);
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
        case OBJ_I64:               MARK_SIMPLE(vm, ObjI64); break;
        case OBJ_U64:               MARK_SIMPLE(vm, ObjU64); break;
        case OBJ_DOUBLE:            MARK_SIMPLE(vm, ObjDouble); break;
        case OBJ_CODE_BLOCK:        markCodeBlock(vm, (ObjCodeBlock*)obj); break;
        case OBJ_VAR_FRAME:         markVarFrame(vm, (ObjVarFrame*)obj); break;
        case OBJ_CALL_FRAME:        markCallFrame(vm, (ObjCallFrame*)obj); break;
        case OBJ_HANDLE_FRAME:      markHandleFrame(vm, (ObjHandleFrame*)obj); break;
        case OBJ_CLOSURE:           markClosure( vm, (ObjClosure*) obj); break;
        case OBJ_CONTINUATION:      markContinuation(vm, (ObjContinuation*)obj); break;
        case OBJ_FIBER:             markFiber(vm, (ObjFiber*)obj); break;
        case OBJ_STRING:            markString(vm, (ObjString*)obj); break;
        case OBJ_FOREIGN:           markForeign(vm, (ObjForeign*)obj); break;
        case OBJ_C_POINTER:         MARK_SIMPLE(vm, ObjCPointer); break;
        case OBJ_LIST:              markList(vm, (ObjList*)obj); break;
        case OBJ_FOREIGN_RESUME:    markForeignResume(vm, (ForeignResume*)obj); break;
        case OBJ_ARRAY:             markArray(vm, (ObjArray*)obj); break;
        case OBJ_BYTE_ARRAY:        MARK_SIMPLE(vm, ObjByteArray); break;
        case OBJ_SLICE:             markSlice(vm, (ObjSlice*)obj); break;
        case OBJ_BYTE_SLICE:        markByteSlice(vm, (ObjByteSlice*)obj); break;
        case OBJ_REF:               markRef(vm, (ObjRef*)obj); break;
        case OBJ_STRUCT:            markStruct(vm, (ObjStruct*)obj); break;
    }
}

void mochiBlackenObjects(MochiVM* vm) {
    while (vm->grayCount > 0) {
        // Pop an item from the gray stack.
        Obj* obj = vm->gray[--vm->grayCount];
        blackenObject(vm, obj);
    }
}