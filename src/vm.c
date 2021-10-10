#include <stdio.h>
#include <string.h>

#include "common.h"
#include "debug.h"
#include "memory.h"
#include "vm.h"

#if ZHENZHU_DEBUG_TRACE_MEMORY || ZHENZHU_DEBUG_TRACE_GC
    #include <time.h>
#endif

// The behavior of realloc() when the size is 0 is implementation defined. It
// may return a non-NULL pointer which must not be dereferenced but nevertheless
// should be freed. To prevent that, we avoid calling realloc() with a zero
// size.
static void* defaultReallocate(void* ptr, size_t newSize, void* _) {
    if (newSize == 0) {
        free(ptr);
        return NULL;
    }
    return realloc(ptr, newSize);
}

#define ALLOCATE_OBJ(type, objectType, vm) (type*)allocateObject(sizeof(type), (objectType), (vm))

static Obj* allocateObject(size_t size, ObjType type, ZZVM* vm) {
    Obj* object = (Obj*)zzReallocate(vm, NULL, 0, size);
    memset(object, 0, size);
    object->type = type;
    // keep track of all allocated objects via the linked list in the vm
    object->next = vm->objects;
    vm->objects = object;
    return object;
}

int zzGetVersionNumber() { 
    return ZHENZHU_VERSION_NUMBER;
}

void zzInitConfiguration(ZhenzhuConfiguration* config) {
    config->reallocateFn = defaultReallocate;
    config->errorFn = NULL;
    config->initialHeapSize = 1024 * 1024 * 10;
    config->minHeapSize = 1024 * 1024;
    config->heapGrowthPercent = 50;
    config->userData = NULL;
}

ZZVM* zzNewVM(ZhenzhuConfiguration* config) {
    ZhenzhuReallocateFn reallocate = defaultReallocate;
    void* userData = NULL;
    if (config != NULL) {
        userData = config->userData;
        reallocate = config->reallocateFn ? config->reallocateFn : defaultReallocate;
    }
    
    ZZVM* vm = (ZZVM*)reallocate(NULL, sizeof(*vm), userData);
    //memset(vm, 0, sizeof(ZZVM));

    // Copy the configuration if given one.
    if (config != NULL) {
        memcpy(&vm->config, config, sizeof(ZhenzhuConfiguration));

        // We choose to set this after copying, 
        // rather than modifying the user config pointer
        vm->config.reallocateFn = reallocate;
    } else {
        zzInitConfiguration(&vm->config);
    }

    vm->block = ALLOCATE_OBJ(ObjCodeBlock, OBJ_CODE_BLOCK, vm);
    zzValueBufferInit(&vm->block->constants);
    zzByteBufferInit(&vm->block->code);
    zzIntBufferInit(&vm->block->lines);

    vm->stackTop = vm->stack;
    vm->callStackTop = vm->callStack;

    // TODO: Should we allocate and free this during a GC?
    vm->grayCount = 0;
    // TODO: Tune this.
    vm->grayCapacity = 4;
    vm->gray = (Obj**)reallocate(NULL, vm->grayCapacity * sizeof(Obj*), userData);
    vm->nextGC = vm->config.initialHeapSize;

    return vm;
}

void zzFreeVM(ZZVM* vm) {
    // Free all of the GC objects.
    Obj* obj = vm->objects;
    while (obj != NULL) {
        Obj* next = obj->next;
        zzFreeObj(vm, obj);
        obj = next;
    }

    // Free up the GC gray set.
    vm->gray = (Obj**)vm->config.reallocateFn(vm->gray, 0, vm->config.userData);

    DEALLOCATE(vm, vm);
}

void zzCollectGarbage(ZZVM* vm) {
#if ZHENZHU_DEBUG_TRACE_MEMORY || ZHENZHU_DEBUG_TRACE_GC
    printf("-- gc --\n");

    size_t before = vm->bytesAllocated;
    double startTime = (double)clock() / CLOCKS_PER_SEC;
#endif

    // Mark all reachable objects.

    // Reset this. As we mark objects, their size will be counted again so that
    // we can track how much memory is in use without needing to know the size
    // of each *freed* object.
    //
    // This is important because when freeing an unmarked object, we don't always
    // know how much memory it is using. For example, when freeing an instance,
    // we need to know its class to know how big it is, but its class may have
    // already been freed.
    vm->bytesAllocated = 0;

    // Temporary roots.
    for (int i = 0; i < vm->numTempRoots; i++) {
        zzGrayObj(vm, vm->tempRoots[i]);
    }

    // Stack variables.
    for (Value* slot = vm->stack; slot < vm->stackTop; slot++) {
        zzGrayValue(vm, *slot);
    }

    // Call stack frames.
    for (ObjVarFrame** slot = vm->callStack; slot < vm->callStackTop; slot++) {
        zzGrayObj(vm, (Obj*)*slot);
    }

    zzGrayObj(vm, (Obj*)vm->block);
    // The current fiber.
    if (vm->fiber != NULL) {
        zzGrayObj(vm, (Obj*)vm->fiber);
    }

    // Now that we have grayed the roots, do a depth-first search over all of the
    // reachable objects.
    zzBlackenObjects(vm);

    // Collect the white objects.
    unsigned long freed = 0;
    unsigned long reachable = 0;
    Obj** obj = &vm->objects;
    while (*obj != NULL) {
        if (!((*obj)->isMarked)) {
            // This object wasn't reached, so remove it from the list and free it.
            Obj* unreached = *obj;
            *obj = unreached->next;
            zzFreeObj(vm, unreached);
            freed += 1;
        } else {
            // This object was reached, so unmark it (for the next GC) and move on to
            // the next.
            (*obj)->isMarked = false;
            obj = &(*obj)->next;
            reachable += 1;
        }
    }

    // Calculate the next gc point, this is the current allocation plus
    // a configured percentage of the current allocation.
    vm->nextGC = vm->bytesAllocated + ((vm->bytesAllocated * vm->config.heapGrowthPercent) / 100);
    if (vm->nextGC < vm->config.minHeapSize) vm->nextGC = vm->config.minHeapSize;

#if ZHENZHU_DEBUG_TRACE_MEMORY || ZHENZHU_DEBUG_TRACE_GC
    double elapsed = ((double)clock() / CLOCKS_PER_SEC) - startTime;
    // Explicit cast because size_t has different sizes on 32-bit and 64-bit and
    // we need a consistent type for the format string.
    printf("GC %lu reachable, %lu freed. Took %.3fms.\nGC %lu before, %lu after (~%lu collected), next at %lu.\n",
            reachable,
            freed,
            elapsed*1000.0,
            (unsigned long)before,
            (unsigned long)vm->bytesAllocated,
            (unsigned long)(before - vm->bytesAllocated),
            (unsigned long)vm->nextGC);
#endif
}

void freeObjects(ZZVM* vm) {
    Obj* object = vm->objects;
    while (object != NULL) {
        Obj* next = object->next;
        zzFreeObj(vm, object);
        object = next;
    }
}

int addConstant(ZZVM* vm, Value value) {
    if (IS_OBJ(value)) { zzPushRoot(vm, AS_OBJ(value)); }
    zzValueBufferWrite(vm, &vm->block->constants, value);
    if (IS_OBJ(value)) { zzPopRoot(vm); }
    return vm->block->constants.count - 1;
}

void writeChunk(ZZVM* vm, uint8_t instr, int line) {
    zzByteBufferWrite(vm, &vm->block->code, instr);
    zzIntBufferWrite(vm, &vm->block->lines, line);
}

//static Value peek(int distance, ZZVM * vm) {
//    return vm->stackTop[-1 - distance];
//}

// Dispatcher function to run the current chunk in the given vm.
static ZhenzhuInterpretResult run(ZZVM * vm) {
#define READ_BYTE() (*vm->ip++)
#define READ_CONSTANT() (vm->block->constants.data[READ_BYTE()])
#define BINARY_OP(valueType, op) \
    do { \
        double b = AS_NUMBER(pop(vm)); \
        double a = AS_NUMBER(pop(vm)); \
        push(valueType(a op b), vm); \
    } while (false)

    for (;;) {
#ifdef ZHENZHU_DEBUG_TRACE_EXECUTION
        disassembleInstruction(vm, (int)(vm->ip - vm->block->code.data));
        printf("STACK:    ");
        if (vm->stack >= vm->stackTop) { printf("<empty>"); }
        for (Value * slot = vm->stack; slot < vm->stackTop; slot++) {
            printf("[ ");
            printValue(*slot);
            printf(" ]");
        }
        printf("\n");
        printf("FRAMES:   ");
        if (vm->callStack >= vm->callStackTop) { printf("<empty>"); }
        for (ObjVarFrame** frame = vm->callStack; frame < vm->callStackTop; frame++) {
            printf("[ ");
            printObject(OBJ_VAL(*frame));
            printf(" ]");
        }
        printf("\n");
#endif
        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
            case OP_NOP: {
                return ZHENZHU_RESULT_SUCCESS;
            }
            case OP_CONSTANT: {
                Value constant = READ_CONSTANT();
                push(constant, vm);
                break;
            }
            case OP_NEGATE:     push(NUMBER_VAL(-AS_NUMBER(pop(vm))), vm); break;
            case OP_ADD:        BINARY_OP(NUMBER_VAL, +); break;
            case OP_SUBTRACT:   BINARY_OP(NUMBER_VAL, -); break;
            case OP_MULTIPLY:   BINARY_OP(NUMBER_VAL, *); break;
            case OP_DIVIDE:     BINARY_OP(NUMBER_VAL, /); break;
            case OP_EQUAL:      BINARY_OP(BOOL_VAL, ==); break;
            case OP_GREATER:    BINARY_OP(BOOL_VAL, >); break;
            case OP_LESS:       BINARY_OP(BOOL_VAL, <); break;
            case OP_TRUE:       push(BOOL_VAL(true), vm); break;
            case OP_FALSE:      push(BOOL_VAL(false), vm); break;
            case OP_NOT:        push(BOOL_VAL(!AS_BOOL(pop(vm))), vm); break;
            case OP_CONCAT: {
                ObjString* b = AS_STRING(pop(vm));
                ObjString* a = AS_STRING(pop(vm));
                zzPushRoot(vm, (Obj*)a);
                zzPushRoot(vm, (Obj*)b);

                int length = a->length + b->length;
                char* chars = ALLOCATE_ARRAY(vm, char, length + 1);
                memcpy(chars, a->chars, a->length);
                memcpy(chars + a->length, b->chars, b->length);
                chars[length] = '\0';
                zzPopRoot(vm);
                zzPopRoot(vm);

                ObjString* result = takeString(chars, length, vm);
                push(OBJ_VAL(result), vm);
                break;
            }

            case OP_STORE: {
                uint8_t varCount = READ_BYTE();
                Value* vars = ALLOCATE_ARRAY(vm, Value, varCount);
                for (int i = 0; i < (int)varCount; i++) {
                    vars[i] = *(vm->stackTop - i);
                }

                ObjVarFrame* frame = newVarFrame(vars, varCount, vm);
                pushFrame(frame, vm);

                for (int i = 0; i < (int)varCount; i++) {
                    pop(vm);
                }
                break;
            }
            case OP_OVERWRITE: {
                printf("TODO\n");
                break;
            }
            case OP_FORGET: {
                popFrame(vm);
                break;
            }
        }
    }

#undef READ_BYTE
#undef READ_CONSTANT
}

ZhenzhuInterpretResult zzInterpret(ZZVM * vm) {
    vm->ip = vm->block->code.data;
    return run(vm);
}

void push(Value value, ZZVM * vm) {
    *vm->stackTop = value;
    vm->stackTop++;
}

Value pop(ZZVM * vm) {
    vm->stackTop--;
    return *vm->stackTop;
}

void pushFrame(ObjVarFrame* frame, ZZVM* vm) {
    *vm->callStackTop = frame;
    vm->callStackTop++;
}

ObjVarFrame* popFrame(ZZVM* vm) {
    vm->callStackTop--;
    return *vm->callStackTop;
}

void zzPushRoot(ZZVM* vm, Obj* obj)
{
  ASSERT(obj != NULL, "Can't root NULL.");
  ASSERT(vm->numTempRoots < ZHENZHU_MAX_TEMP_ROOTS, "Too many temporary roots.");

  vm->tempRoots[vm->numTempRoots++] = obj;
}

void zzPopRoot(ZZVM* vm)
{
  ASSERT(vm->numTempRoots > 0, "No temporary roots to release.");
  vm->numTempRoots--;
}




static ObjString* allocateString(char* chars, int length, ZZVM* vm) {
    ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING, vm);
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
    ObjVarFrame* frame = ALLOCATE_OBJ(ObjVarFrame, OBJ_VAR_FRAME, vm);
    frame->slots = vars;
    frame->slotCount = varCount;
    return frame;
}

ObjCallFrame* newCallFrame(Value* vars, int varCount, uint8_t* afterLocation, ZZVM* vm) {
    ObjCallFrame* frame = ALLOCATE_OBJ(ObjCallFrame, OBJ_CALL_FRAME, vm);
    frame->vars.slots = vars;
    frame->vars.slotCount = varCount;
    frame->afterLocation = afterLocation;
    return frame;
}