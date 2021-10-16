#include <stdio.h>
#include <string.h>

#include "common.h"
#include "debug.h"
#include "memory.h"
#include "vm.h"

#if ZHENZHU_DEBUG_TRACE_MEMORY || ZHENZHU_DEBUG_TRACE_GC
    #include <time.h>
#endif

#if ZHENZHU_BATTERY_UV
    #include "uv.h"
    #include "battery_uv.h"
#endif

DEFINE_BUFFER(ForeignFunction, ZhenzhuForeignMethodFn);

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

int zzGetVersionNumber() { 
    return ZHENZHU_VERSION_NUMBER;
}

void zzInitConfiguration(ZhenzhuConfiguration* config) {
    config->reallocateFn = defaultReallocate;
    config->errorFn = NULL;
    config->valueStackCapacity = 128;
    config->frameStackCapacity = 512;
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
    memset(vm, 0, sizeof(ZZVM));

    // Copy the configuration if given one.
    if (config != NULL) {
        memcpy(&vm->config, config, sizeof(ZhenzhuConfiguration));

        // We choose to set this after copying, 
        // rather than modifying the user config pointer
        vm->config.reallocateFn = reallocate;
    } else {
        zzInitConfiguration(&vm->config);
    }

    // TODO: Should we allocate and free this during a GC?
    vm->grayCount = 0;
    // TODO: Tune this.
    vm->grayCapacity = 4;
    vm->gray = (Obj**)reallocate(NULL, vm->grayCapacity * sizeof(Obj*), userData);
    vm->nextGC = vm->config.initialHeapSize;

    vm->block = zzNewCodeBlock(vm);
    zzForeignFunctionBufferInit(&vm->foreignFns);

#if ZHENZHU_BATTERY_UV
    zzAddForeign(vm, uvzzNewTimer);
    zzAddForeign(vm, uvzzCloseTimer);
#endif

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

    zzForeignFunctionBufferClear(vm, &vm->foreignFns);
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

    if (vm->block != NULL) {
        zzGrayObj(vm, (Obj*)vm->block);
    }
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

// Dispatcher function to run a particular fiber in the context of the given vm.
static ZhenzhuInterpretResult run(ZZVM * vm, register ObjFiber* fiber) {

    // Remember the current fiber in case of GC.
    vm->fiber = fiber;
    fiber->isRoot = true;

    register uint8_t* ip = fiber->ip;

#define PUSH_VAL(value) (*fiber->valueStackTop++ = value)
#define POP_VAL()       (*(--fiber->valueStackTop))
#define DROP_VAL()      (--fiber->valueStackTop)
#define PEEK_VAL(index) (*(fiber->valueStackTop - index))

#define PUSH_FRAME(frame)   (*fiber->frameStackTop++ = frame)
#define POP_FRAME()         (*(--fiber->frameStackTop))
#define DROP_FRAME()        (--fiber->frameStackTop)

#define READ_BYTE() (*ip++)
#define READ_SHORT() (ip += 2, (uint16_t)((ip[-2] << 8) | ip[-1]))
#define READ_CONSTANT() (vm->block->constants.data[READ_BYTE()])
#define BINARY_OP(valueType, op) \
    do { \
        double b = AS_NUMBER(POP_VAL()); \
        double a = AS_NUMBER(POP_VAL()); \
        PUSH_VAL(valueType(a op b)); \
    } while (false)

#ifdef ZHENZHU_DEBUG_TRACE_EXECUTION
    #define DEBUG_TRACE_INSTRUCTIONS() \
        do { \
            disassembleInstruction(vm, (int)(ip - vm->block->code.data)); \
            printf("STACK:    "); \
            if (fiber->valueStack >= fiber->valueStackTop) { printf("<empty>"); } \
            for (Value * slot = fiber->valueStack; slot < fiber->valueStackTop; slot++) { \
                printf("[ "); \
                printValue(*slot); \
                printf(" ]"); \
            } \
            printf("\n"); \
            printf("FRAMES:   "); \
            if (fiber->frameStack >= fiber->frameStackTop) { printf("<empty>"); } \
            for (ObjVarFrame** frame = fiber->frameStack; frame < fiber->frameStackTop; frame++) { \
                printf("[ "); \
                printObject(OBJ_VAL(*frame)); \
                printf(" ]"); \
            } \
            printf("\n"); \
        } while (false)
#else
    #define DEBUG_TRACE_INSTRUCTIONS() do { } while (false)
#endif

#if ZHENZHU_BATTERY_UV
    #define UV_EVENT_LOOP()                                                  \
        do {                                                                 \
            printf("Checking for LibUV events.\n");                          \
            uv_run(uv_default_loop(), UV_RUN_NOWAIT);                        \
        } while (false)
#else
    #define UV_EVENT_LOOP() do { } while (false)
#endif

#if ZHENZHU_COMPUTED_GOTO

    static void* dispatchTable[] = {
        #define OPCODE(name) &&code_##name,
        #include "opcodes.h"
        #undef OPCODE
    };

    #define INTERPRET_LOOP    DISPATCH();
    #define CASE_CODE(name)   code_##name

    #define DISPATCH()                                                           \
        do                                                                       \
        {                                                                        \
            DEBUG_TRACE_INSTRUCTIONS();                                          \
            UV_EVENT_LOOP();                                                     \
            if (fiber->isSuspended) { goto CASE_CODE(OP_NOP); }                  \
            goto *dispatchTable[instruction = (Code)READ_BYTE()];                \
        } while (false)

#else

    #define INTERPRET_LOOP                                                       \
        loop:                                                                    \
            DEBUG_TRACE_INSTRUCTIONS();                                          \
            UV_EVENT_LOOP();                                                     \
            if (fiber->isSuspended) { goto loop; }                               \
            switch (instruction = (Code)READ_BYTE())

    #define CASE_CODE(name)  case CODE_##name
    #define DISPATCH()       goto loop

#endif

    Code instruction;
    INTERPRET_LOOP
    {
        CASE_CODE(OP_NOP): {
            printf("NOP\n");
            DISPATCH();
        }
        CASE_CODE(OP_ABORT): {
            uint8_t retCode = READ_BYTE();
            return retCode;
        }
        CASE_CODE(OP_CONSTANT): {
            Value constant = READ_CONSTANT();
            PUSH_VAL(constant);
            DISPATCH();
        }
        CASE_CODE(OP_NEGATE): {
            double n = AS_NUMBER(POP_VAL());
            PUSH_VAL(NUMBER_VAL(-n));
            DISPATCH();
        }
        CASE_CODE(OP_ADD):      BINARY_OP(NUMBER_VAL, +); DISPATCH();
        CASE_CODE(OP_SUBTRACT): BINARY_OP(NUMBER_VAL, -); DISPATCH();
        CASE_CODE(OP_MULTIPLY): BINARY_OP(NUMBER_VAL, *); DISPATCH();
        CASE_CODE(OP_DIVIDE):   BINARY_OP(NUMBER_VAL, /); DISPATCH();
        CASE_CODE(OP_EQUAL):    BINARY_OP(BOOL_VAL, ==); DISPATCH();
        CASE_CODE(OP_GREATER):  BINARY_OP(BOOL_VAL, >); DISPATCH();
        CASE_CODE(OP_LESS):     BINARY_OP(BOOL_VAL, <); DISPATCH();
        CASE_CODE(OP_TRUE):     PUSH_VAL(BOOL_VAL(true)); DISPATCH();
        CASE_CODE(OP_FALSE):    PUSH_VAL(BOOL_VAL(false)); DISPATCH();
        CASE_CODE(OP_NOT): {
            bool b = AS_BOOL(POP_VAL());
            PUSH_VAL(BOOL_VAL(!b));
            DISPATCH();
        }
        CASE_CODE(OP_CONCAT): {
            ObjString* b = AS_STRING(PEEK_VAL(1));
            ObjString* a = AS_STRING(PEEK_VAL(2));

            int length = a->length + b->length;
            char* chars = ALLOCATE_ARRAY(vm, char, length + 1);
            memcpy(chars, a->chars, a->length);
            memcpy(chars + a->length, b->chars, b->length);
            chars[length] = '\0';
            DROP_VAL();
            DROP_VAL();

            ObjString* result = takeString(chars, length, vm);
            PUSH_VAL(OBJ_VAL(result));
            DISPATCH();
        }
        CASE_CODE(OP_STORE): {
            uint8_t varCount = READ_BYTE();
            Value* vars = ALLOCATE_ARRAY(vm, Value, varCount);
            for (int i = 0; i < (int)varCount; i++) {
                vars[i] = *(fiber->valueStackTop - i);
            }

            ObjVarFrame* frame = newVarFrame(vars, varCount, vm);
            PUSH_FRAME(frame);

            for (int i = 0; i < (int)varCount; i++) {
                DROP_VAL();
            }
            DISPATCH();
        }
        CASE_CODE(OP_OVERWRITE): {
            ASSERT(false, "OP_OVERWRITE not yet implemented.");
            DISPATCH();
        }
        CASE_CODE(OP_FORGET): {
            DROP_FRAME();
            DISPATCH();
        }
        CASE_CODE(OP_CALL_FOREIGN): {
            ZhenzhuForeignMethodFn fn = vm->foreignFns.data[READ_BYTE()];
            fn(vm, fiber);
            DISPATCH();
        }
    }

    UNREACHABLE();
    return ZHENZHU_RESULT_RUNTIME_ERROR;

#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
}

ZhenzhuInterpretResult zzInterpret(ZZVM* vm, ObjFiber* fiber) {
    fiber->ip = vm->block->code.data;
    return run(vm, fiber);
}

void zzPushRoot(ZZVM* vm, Obj* obj) {
    ASSERT(obj != NULL, "Can't root NULL.");
    ASSERT(vm->numTempRoots < ZHENZHU_MAX_TEMP_ROOTS, "Too many temporary roots.");

    vm->tempRoots[vm->numTempRoots++] = obj;
}

void zzPopRoot(ZZVM* vm) {
    ASSERT(vm->numTempRoots > 0, "No temporary roots to release.");
    vm->numTempRoots--;
}

int zzAddForeign(ZZVM* vm, ZhenzhuForeignMethodFn fn) {
    zzForeignFunctionBufferWrite(vm, &vm->foreignFns, fn);
    return vm->foreignFns.count - 1;
}