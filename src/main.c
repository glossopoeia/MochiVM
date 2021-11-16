#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "debug.h"
#include "memory.h"
#include "uv.h"
#include "vm.h"
#include <SDL.h>

#define TEST_DOUBLE_VAL(val) DOUBLE_VAL(vm, val)
#define TEST_I32_VAL(val) I32_VAL(vm, val)

#define BEGIN_TEST(header)                                                                                             \
    do {                                                                                                               \
        printf("=============================\n");                                                                     \
        printf(header);                                                                                                \
        printf("\n");                                                                                                  \
        printf("=============================\n");                                                                     \
        vm = mochiNewVM(NULL);                                                                                         \
        ObjFiber *fiber = mochiNewFiber(vm, vm->code.data, NULL, 0);                                                   \
        vm->fiber = fiber;                                                                                             \
        assertNumber = 0;                                                                                              \
        assertString = NULL;                                                                                           \
        assertStack = -1;                                                                                              \
        assertFrames = -1;                                                                                             \
    } while (false);

#define END_TEST()                                                                                                     \
    do {                                                                                                               \
        disassembleChunk(vm, "test chunk");                                                                            \
        mochiInterpret(vm, vm->fiber);                                                                                 \
        if (assertStack >= 0) {                                                                                        \
            ASSERT(assertStack == vm->fiber->valueStackTop - vm->fiber->valueStack,                                    \
                   "TEST FAILED: Unexpected stack count");                                                             \
        }                                                                                                              \
        if (assertFrames >= 0) {                                                                                       \
            ASSERT(assertFrames == vm->fiber->frameStackTop - vm->fiber->frameStack,                                   \
                   "TEST FAILED: Unexpected frame count");                                                             \
        }                                                                                                              \
        if (assertString != NULL) {                                                                                    \
            printf("TODO: String verify unimplemented\n");                                                             \
        }                                                                                                              \
        if (assertNumber != 0) {                                                                                       \
            double num = AS_DOUBLE(*(vm->fiber->valueStackTop - 1));                                                   \
            ASSERT(assertNumber == num, "TEST FAILED: Unexpected number at the end of execution.");                    \
        }                                                                                                              \
        mochiFreeVM(vm);                                                                                               \
        printf("TEST PASSED\n");                                                                                       \
    } while (false);

#define CONSTANT(arg) addConstant(vm, (arg));
#define WRITE_INST(inst, line) writeChunk(vm, CODE_##inst, (line));
#define WRITE_BYTE(byte, line) writeChunk(vm, (byte), (line));
#define WRITE_LABEL(label, length) writeLabel(vm, vm->code.count, length, label)
#define VERIFY_STACK(count) assertStack = (count);
#define VERIFY_FRAMES(count) assertFrames = (count);
#define VERIFY_NUMBER(num) assertNumber = num;
#define VERIFY_STRING(str) assertString = str;

#define WRITE_SHORT(val, line)                                                                                         \
    do {                                                                                                               \
        writeChunk(vm, ((uint16_t)(val)) >> 8, (line));                                                                \
        writeChunk(vm, ((uint16_t)(val)), (line));                                                                     \
    } while (false);

#define WRITE_INT(val, line)                                                                                           \
    do {                                                                                                               \
        writeChunk(vm, (val) >> 24, (line));                                                                           \
        writeChunk(vm, (val) >> 16, (line));                                                                           \
        writeChunk(vm, (val) >> 8, (line));                                                                            \
        writeChunk(vm, (val), (line));                                                                                 \
    } while (false);

#define WRITE_INT_INST(inst, arg, line)                                                                                \
    do {                                                                                                               \
        writeChunk(vm, CODE_##inst, (line));                                                                           \
        writeChunk(vm, (arg) >> 24, (line));                                                                           \
        writeChunk(vm, (arg) >> 16, (line));                                                                           \
        writeChunk(vm, (arg) >> 8, (line));                                                                            \
        writeChunk(vm, (arg), (line));                                                                                 \
    } while (false);

int main(int argc, const char *argv[]) {
    printf("MochiVM is under development... watch for bugs!\n");

#if DEBUG
    MochiVM *vm = NULL;
    double assertNumber = 0;
    char *assertString = NULL;
    int assertStack = -1;
    int assertFrames = -1;

#include "test_conds.h"
#include "test_frames.h"
#include "test_handle.h"
#include "test_numerics.h"
#include "test_refs.h"
#if MOCHIVM_BATTERY_UV && MOCHIVM_BATTERY_SDL
#include "test_foreign.h"
#endif
#endif

    return 0;
}