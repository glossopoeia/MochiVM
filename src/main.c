#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "vm.h"
#include "debug.h"
#include "memory.h"
#include "uv.h"

#define BEGIN_TEST(header)      \
    do {                        \
        printf("=============================\n"); \
        printf(header); \
        printf("\n"); \
        printf("=============================\n"); \
        vm = zzNewVM(NULL);     \
        assertNumber = 0;       \
        assertString = NULL;    \
        assertStack = -1;       \
        assertFrames = -1;      \
    } while (false);

#define END_TEST() \
    do { \
        disassembleChunk(vm, "test chunk"); \
        ObjFiber* fiber = zzNewFiber(vm, vm->block->code.data, NULL, 0); \
        zzInterpret(vm, fiber); \
        if (assertStack >= 0) { \
            ASSERT(assertStack == fiber->valueStackTop - fiber->valueStack, "TEST FAILED: Unexpected stack count"); \
        } \
        if (assertFrames >= 0) { \
            ASSERT(assertFrames == fiber->frameStackTop - fiber->frameStack, "TEST FAILED: Unexpected frame count"); \
        } \
        if (assertString != NULL) { \
            printf("TODO: String verify unimplemented\n"); \
        } \
        if (assertNumber != 0) { \
            printf("TODO: Number verify unimplemented\n"); \
        } \
        zzFreeVM(vm); \
        printf("TEST PASSED\n"); \
    } while (false);

#define CONSTANT(arg)               addConstant(vm, (arg));
#define WRITE_INST(inst, line)      writeChunk(vm, CODE_##inst, (line));
#define WRITE_BYTE(byte, line)      writeChunk(vm, (byte), (line));
#define VERIFY_STACK(count)         assertStack = (count);
#define VERIFY_FRAMES(count)        assertFrames = (count);
#define VERIFY_NUMBER(num)          assertNumber = num;
#define VERIFY_STRING(str)          assertString = str;

int main(int argc, const char * argv[]) {
    printf("Zhenzhu VM is under development... watch for bugs!\n");

    ZZVM* vm = NULL;
    double assertNumber = 0;
    char* assertString = NULL;
    int assertStack = -1;
    int assertFrames = -1;

    #include "test_numerics.h"
    #include "test_strings.h"
#if ZHENZHU_BATTERY_UV
    #include "test_foreign.h"
#endif

#if ZHENZHU_BATTERY_UV
    printf("Terminating LibUV default loop.\n");
    uv_loop_close(uv_default_loop());
#endif

    return 0;
}