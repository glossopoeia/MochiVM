#include <stdio.h>
#include <string.h>

#include "common.h"
#include "debug.h"
#include "memory.h"
#include "vm.h"

#if MOCHIVM_DEBUG_TRACE_MEMORY || MOCHIVM_DEBUG_TRACE_GC
    #include <time.h>
#endif

#if MOCHIVM_BATTERY_UV
    #include "uv.h"
    #include "battery_uv.h"
#endif

DEFINE_BUFFER(ForeignFunction, MochiVMForeignMethodFn);

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

int mochiGetVersionNumber() { 
    return MOCHIVM_VERSION_NUMBER;
}

void mochiInitConfiguration(MochiVMConfiguration* config) {
    config->reallocateFn = defaultReallocate;
    config->errorFn = NULL;
    config->valueStackCapacity = 128;
    config->frameStackCapacity = 512;
    config->initialHeapSize = 1024 * 1024 * 10;
    config->minHeapSize = 1024 * 1024;
    config->heapGrowthPercent = 50;
    config->userData = NULL;
}

MochiVM* mochiNewVM(MochiVMConfiguration* config) {
    MochiVMReallocateFn reallocate = defaultReallocate;
    void* userData = NULL;
    if (config != NULL) {
        userData = config->userData;
        reallocate = config->reallocateFn ? config->reallocateFn : defaultReallocate;
    }
    
    MochiVM* vm = (MochiVM*)reallocate(NULL, sizeof(*vm), userData);
    memset(vm, 0, sizeof(MochiVM));

    // Copy the configuration if given one.
    if (config != NULL) {
        memcpy(&vm->config, config, sizeof(MochiVMConfiguration));

        // We choose to set this after copying, 
        // rather than modifying the user config pointer
        vm->config.reallocateFn = reallocate;
    } else {
        mochiInitConfiguration(&vm->config);
    }

    // TODO: Should we allocate and free this during a GC?
    vm->grayCount = 0;
    // TODO: Tune this.
    vm->grayCapacity = 4;
    vm->gray = (Obj**)reallocate(NULL, vm->grayCapacity * sizeof(Obj*), userData);
    vm->nextGC = vm->config.initialHeapSize;

    vm->block = mochiNewCodeBlock(vm);
    mochiForeignFunctionBufferInit(&vm->foreignFns);

#if MOCHIVM_BATTERY_UV
    mochiAddForeign(vm, uvmochiNewTimer);
    mochiAddForeign(vm, uvmochiCloseTimer);
#endif

    return vm;
}

void mochiFreeVM(MochiVM* vm) {

    // Free all of the GC objects.
    Obj* obj = vm->objects;
    while (obj != NULL) {
        Obj* next = obj->next;
        mochiFreeObj(vm, obj);
        obj = next;
    }

    // Free up the GC gray set.
    vm->gray = (Obj**)vm->config.reallocateFn(vm->gray, 0, vm->config.userData);

    mochiForeignFunctionBufferClear(vm, &vm->foreignFns);
    DEALLOCATE(vm, vm);
}

void mochiCollectGarbage(MochiVM* vm) {
#if MOCHIVM_DEBUG_TRACE_MEMORY || MOCHIVM_DEBUG_TRACE_GC
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
        mochiGrayObj(vm, vm->tempRoots[i]);
    }

    if (vm->block != NULL) {
        mochiGrayObj(vm, (Obj*)vm->block);
    }
    // The current fiber.
    if (vm->fiber != NULL) {
        mochiGrayObj(vm, (Obj*)vm->fiber);
    }

    // Now that we have grayed the roots, do a depth-first search over all of the
    // reachable objects.
    mochiBlackenObjects(vm);

    // Collect the white objects.
    unsigned long freed = 0;
    unsigned long reachable = 0;
    Obj** obj = &vm->objects;
    while (*obj != NULL) {
        if (!((*obj)->isMarked)) {
            // This object wasn't reached, so remove it from the list and free it.
            Obj* unreached = *obj;
            *obj = unreached->next;
            mochiFreeObj(vm, unreached);
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

#if MOCHIVM_DEBUG_TRACE_MEMORY || MOCHIVM_DEBUG_TRACE_GC
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

int addConstant(MochiVM* vm, Value value) {
    if (IS_OBJ(value)) { mochiPushRoot(vm, AS_OBJ(value)); }
    mochiValueBufferWrite(vm, &vm->block->constants, value);
    if (IS_OBJ(value)) { mochiPopRoot(vm); }
    return vm->block->constants.count - 1;
}

void writeChunk(MochiVM* vm, uint8_t instr, int line) {
    mochiByteBufferWrite(vm, &vm->block->code, instr);
    mochiIntBufferWrite(vm, &vm->block->lines, line);
}

// Dispatcher function to run a particular fiber in the context of the given vm.
static MochiVMInterpretResult run(MochiVM * vm, register ObjFiber* fiber) {

    // Remember the current fiber in case of GC.
    vm->fiber = fiber;
    fiber->isRoot = true;

    register uint8_t* ip = fiber->ip;
    register uint8_t* codeStart = vm->block->code.data;

#define FROM_START(offset)  (codeStart + (int)(offset))

#define PUSH_VAL(value)     (*fiber->valueStackTop++ = value)
#define POP_VAL()           (*(--fiber->valueStackTop))
#define DROP_VALS(count)    (fiber->valueStackTop = fiber->valueStackTop - (count))
#define PEEK_VAL(index)     (*(fiber->valueStackTop - index))

#define PUSH_FRAME(frame)   (*fiber->frameStackTop++ = (ObjVarFrame*)(frame))
#define POP_FRAME()         (*(--fiber->frameStackTop))
#define DROP_FRAMES(count)  (fiber->frameStackTop = fiber->frameStackTop - (count))
#define PEEK_FRAME(index)   (*(fiber->frameStackTop - index))
#define FIND(frame, slot)   ((*(fiber->frameStackTop - 1 - (frame)))->slots[(slot)])

#define READ_BYTE() (*ip++)
#define READ_SHORT() (ip += 2, (int16_t)((ip[-2] << 8) | ip[-1]))
#define READ_USHORT() (ip += 2, (uint16_t)((ip[-2] << 8) | ip[-1]))
#define READ_UINT() (ip += 4, (uint32_t)((ip[-4] << 24) | (ip[-3] << 16) | (ip[-2] << 8) | ip[-1]))
#define READ_CONSTANT() (vm->block->constants.data[READ_BYTE()])
#define BINARY_OP(valueType, op) \
    do { \
        double b = AS_NUMBER(POP_VAL()); \
        double a = AS_NUMBER(POP_VAL()); \
        PUSH_VAL(valueType(a op b)); \
    } while (false)

#ifdef MOCHIVM_DEBUG_TRACE_EXECUTION
    #define DEBUG_TRACE_INSTRUCTIONS() \
        do { \
            disassembleInstruction(vm, (int)(ip - codeStart)); \
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

#if MOCHIVM_BATTERY_UV
    #define UV_EVENT_LOOP()                                                  \
        do {                                                                 \
            printf("Checking for LibUV events.\n");                          \
            uv_run(uv_default_loop(), UV_RUN_NOWAIT);                        \
        } while (false)
#else
    #define UV_EVENT_LOOP() do { } while (false)
#endif

#if MOCHIVM_COMPUTED_GOTO

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
            if (fiber->isSuspended) { goto CASE_CODE(NOP); }                  \
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
        CASE_CODE(NOP): {
            printf("NOP\n");
            DISPATCH();
        }
        CASE_CODE(ABORT): {
            uint8_t retCode = READ_BYTE();
            return retCode;
        }
        CASE_CODE(CONSTANT): {
            Value constant = READ_CONSTANT();
            PUSH_VAL(constant);
            DISPATCH();
        }
        CASE_CODE(NEGATE): {
            double n = AS_NUMBER(POP_VAL());
            PUSH_VAL(NUMBER_VAL(-n));
            DISPATCH();
        }
        CASE_CODE(ADD):      BINARY_OP(NUMBER_VAL, +); DISPATCH();
        CASE_CODE(SUBTRACT): BINARY_OP(NUMBER_VAL, -); DISPATCH();
        CASE_CODE(MULTIPLY): BINARY_OP(NUMBER_VAL, *); DISPATCH();
        CASE_CODE(DIVIDE):   BINARY_OP(NUMBER_VAL, /); DISPATCH();
        CASE_CODE(EQUAL):    BINARY_OP(BOOL_VAL, ==); DISPATCH();
        CASE_CODE(GREATER):  BINARY_OP(BOOL_VAL, >); DISPATCH();
        CASE_CODE(LESS):     BINARY_OP(BOOL_VAL, <); DISPATCH();
        CASE_CODE(TRUE):     PUSH_VAL(BOOL_VAL(true)); DISPATCH();
        CASE_CODE(FALSE):    PUSH_VAL(BOOL_VAL(false)); DISPATCH();
        CASE_CODE(NOT): {
            bool b = AS_BOOL(POP_VAL());
            PUSH_VAL(BOOL_VAL(!b));
            DISPATCH();
        }
        CASE_CODE(CONCAT): {
            ObjString* b = AS_STRING(PEEK_VAL(1));
            ObjString* a = AS_STRING(PEEK_VAL(2));

            int length = a->length + b->length;
            char* chars = ALLOCATE_ARRAY(vm, char, length + 1);
            memcpy(chars, a->chars, a->length);
            memcpy(chars + a->length, b->chars, b->length);
            chars[length] = '\0';
            DROP_VALS(2);

            ObjString* result = takeString(chars, length, vm);
            PUSH_VAL(OBJ_VAL(result));
            DISPATCH();
        }
        CASE_CODE(STORE): {
            uint8_t varCount = READ_BYTE();
            Value* vars = ALLOCATE_ARRAY(vm, Value, varCount);
            for (int i = 0; i < (int)varCount; i++) {
                vars[i] = *(fiber->valueStackTop - i);
            }

            ObjVarFrame* frame = newVarFrame(vars, varCount, vm);
            PUSH_FRAME(frame);

            DROP_VALS(varCount);
            DISPATCH();
        }
        CASE_CODE(OVERWRITE): {
            ASSERT(false, "OVERWRITE not yet implemented.");
            DISPATCH();
        }
        CASE_CODE(FORGET): {
            DROP_FRAMES(1);
            DISPATCH();
        }

        CASE_CODE(CALL_FOREIGN): {
            MochiVMForeignMethodFn fn = vm->foreignFns.data[READ_SHORT()];
            fn(vm, fiber);
            DISPATCH();
        }
        CASE_CODE(CALL): {
            uint8_t* callPtr = FROM_START(READ_UINT());
            ObjCallFrame* frame = newCallFrame(NULL, 0, ip, vm);
            PUSH_FRAME((ObjVarFrame*)frame);
            ip = callPtr;
            DISPATCH();
        }
        CASE_CODE(TAILCALL): {
            ip = FROM_START(READ_UINT());
            DISPATCH();
        }
        CASE_CODE(CALL_CLOSURE): {
            // only peek the closure to make sure it gets traced by garbage collection
            ObjClosure* closure = (ObjClosure*)PEEK_VAL(1);

            // need to populate the frame with the captured values, but also the parameters from the stack
            // top of the stack is first in the frame, next is second, etc.
            // captured are copied as they appear in the closure
            Value* frameVars = ALLOCATE_ARRAY(vm, Value, closure->paramCount + closure->capturedCount);
            for (int i = 0; i < closure->paramCount; i++) {
                frameVars[i] = PEEK_VAL(i+1);
            }
            memcpy(frameVars + closure->paramCount, closure->captured, closure->capturedCount);
            ObjCallFrame* frame = newCallFrame(frameVars, closure->paramCount + closure->capturedCount, ip, vm);

            // jump to the closure body, drop the now-stored stack values, and push the frame
            ip = closure->funcLocation;
            DROP_VALS(1 + closure->paramCount);
            PUSH_FRAME(frame);
            DISPATCH();
        }
        CASE_CODE(TAILCALL_CLOSURE): {
            // only peek the closure to make sure it gets traced by garbage collection
            ObjClosure* closure = (ObjClosure*)PEEK_VAL(1);

            // pop the old frame and create a new frame with the new array of stored values but the same return location
            ObjCallFrame* oldFrame = (ObjCallFrame*)PEEK_FRAME(1);
            Value* frameVars = ALLOCATE_ARRAY(vm, Value, closure->paramCount + closure->capturedCount);
            for (int i = 0; i < closure->paramCount; i++) {
                frameVars[i] = PEEK_VAL(i+1);
            }
            memcpy(frameVars + closure->paramCount, closure->captured, closure->capturedCount);
            ObjCallFrame* frame = newCallFrame(frameVars, closure->paramCount + closure->capturedCount, oldFrame->afterLocation, vm);

            // jump to the closure body, drop the old frame, drop the now-stored stack values, and push the new frame
            ip = closure->funcLocation;
            DROP_FRAMES(1);
            DROP_VALS(1 + closure->paramCount);
            PUSH_FRAME(frame);
            DISPATCH();
        }
        CASE_CODE(OFFSET): {
            ip = ip + (int)READ_SHORT();
            DISPATCH();
        }
        CASE_CODE(RETURN): {
            ObjCallFrame* frame = (ObjCallFrame*)POP_FRAME();
            ip = frame->afterLocation;
            DISPATCH();
        }
        CASE_CODE(CLOSURE): {
            uint8_t* bodyLocation = FROM_START(READ_UINT());
            uint8_t paramCount = READ_BYTE();
            uint16_t closedCount = READ_USHORT();
            ObjClosure* closure = mochiNewClosure(vm, bodyLocation, paramCount, closedCount);
            for (int i = 0; i < closedCount; i++) {
                uint16_t frame = READ_USHORT();
                uint16_t slot = READ_USHORT();
                mochiClosureCapture(closure, i, FIND(frame, slot));
            }
            PUSH_VAL(OBJ_VAL(closure));
            DISPATCH();
        }
        CASE_CODE(RECURSIVE): {
            uint8_t* bodyLocation = FROM_START(READ_UINT());
            uint8_t paramCount = READ_BYTE();
            uint16_t closedCount = READ_USHORT();
            // add one to closed count to save a slot for the closure itself
            ObjClosure* closure = mochiNewClosure(vm, bodyLocation, paramCount, closedCount + 1);
            // capture everything listed in the instruction args, saving the first spot for the closure itself
            mochiClosureCapture(closure, 0, OBJ_VAL(closure));
            for (int i = 0; i < closedCount; i++) {
                uint16_t frame = READ_USHORT();
                uint16_t slot = READ_USHORT();
                mochiClosureCapture(closure, i + 1, FIND(frame, slot));
            }
            PUSH_VAL(OBJ_VAL(closure));
            DISPATCH();
        }
        CASE_CODE(MUTUAL): {
            ASSERT(false, "MUTUAL not yet implemented.");
        }
        CASE_CODE(HANDLE): {
            ASSERT(false, "HANDLE not yet implemented.");
        }
        CASE_CODE(COMPLETE): {
            ObjMarkFrame* frame = (ObjMarkFrame*)PEEK_FRAME(1);
            ObjClosure* afterClosure = AS_CLOSURE(frame->afterClosure);
            // TODO: remove this limitation
            ASSERT(afterClosure->paramCount == 0, "After-closure parameter count must be 0 in handler completion.");
            int varCount = frame->call.vars.slotCount + afterClosure->capturedCount;
            Value* vars = ALLOCATE_CONCAT(vm, Value,
                frame->call.vars.slots, frame->call.vars.slotCount,
                afterClosure->captured, afterClosure->capturedCount);
            ObjCallFrame* newFrame = newCallFrame(vars, varCount, frame->call.afterLocation, vm);

            DROP_FRAMES(1);
            PUSH_FRAME(newFrame);
            ip = afterClosure->funcLocation;
            DISPATCH();
        }
        CASE_CODE(ESCAPE): {
            ASSERT(false, "ESCAPE not yet implemented.");
        }
        CASE_CODE(REACT): {
            ASSERT(false, "REACT not yet implemented.");
        }
        CASE_CODE(CALL_CONTINUATION): {
            ASSERT(false, "CALL_CONTINUATION not yet implemented.");
        }
        CASE_CODE(TAILCALL_CONTINUATION): {
            ASSERT(false, "TAILCALL_CONTINUATION not yet implemented.");
        }
    }

    UNREACHABLE();
    return MOCHIVM_RESULT_RUNTIME_ERROR;

#undef READ_BYTE
#undef READ_SHORT
#undef READ_USHORT
#undef READ_CONSTANT
}

MochiVMInterpretResult mochiInterpret(MochiVM* vm, ObjFiber* fiber) {
    fiber->ip = vm->block->code.data;
    return run(vm, fiber);
}

void mochiPushRoot(MochiVM* vm, Obj* obj) {
    ASSERT(obj != NULL, "Can't root NULL.");
    ASSERT(vm->numTempRoots < MOCHIVM_MAX_TEMP_ROOTS, "Too many temporary roots.");

    vm->tempRoots[vm->numTempRoots++] = obj;
}

void mochiPopRoot(MochiVM* vm) {
    ASSERT(vm->numTempRoots > 0, "No temporary roots to release.");
    vm->numTempRoots--;
}

int mochiAddForeign(MochiVM* vm, MochiVMForeignMethodFn fn) {
    mochiForeignFunctionBufferWrite(vm, &vm->foreignFns, fn);
    return vm->foreignFns.count - 1;
}