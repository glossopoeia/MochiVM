#include <stdio.h>
#include <stdlib.h>

#include "debug.h"
#include <SDL.h>

#define BEGIN_TEST(header)                                                                                             \
    do {                                                                                                               \
        printf("=============================\n");                                                                     \
        printf(header);                                                                                                \
        printf("\n");                                                                                                  \
        printf("=============================\n");                                                                     \
        vm = mochiNewVM(NULL);                                                                                         \
        assertNumber = 0;                                                                                              \
        assertString = NULL;                                                                                           \
        assertStack = -1;                                                                                              \
        assertFrames = -1;                                                                                             \
    } while (false);

#define END_TEST()                                                                                                     \
    do {                                                                                                               \
        disassembleChunk(vm, "test chunk");                                                                            \
        mochiInterpret(vm);                                                                                            \
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

#define CONST_DOUBLE(arg)      mochiWriteDoubleConst(vm, (arg));
#define CONST_I32(arg)         mochiWriteI32Const(vm, (arg));
#define WRITE_INST(inst, line) mochiWriteCodeByte(vm, CODE_##inst, (line));
#define WRITE_BYTE(byte, line) mochiWriteCodeByte(vm, (byte), (line));
#define WRITE_LABEL(label)     mochiWriteLabel(vm, vm->code.count, label)
#define VERIFY_STACK(count)    assertStack = (count);
#define VERIFY_FRAMES(count)   assertFrames = (count);
#define VERIFY_NUMBER(num)     assertNumber = num;
#define VERIFY_STRING(str)     assertString = str;

#define WRITE_SHORT(val, line) mochiWriteCodeU16(vm, val, line);
#define WRITE_INT(val, line)   mochiWriteCodeI32(vm, val, line);

#define WRITE_INT_INST(inst, arg, line)                                                                                \
    do {                                                                                                               \
        mochiWriteCodeByte(vm, CODE_##inst, (line));                                                                   \
        mochiWriteCodeI32(vm, arg, line);                                                                              \
    } while (false);

int main(int argc, const char* argv[]) {
    printf("MochiVM is under development... watch for bugs!\n");

#if DEBUG
    MochiVM* vm = NULL;
    double assertNumber = 0;
    char* assertString = NULL;
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