#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "vm.h"
#include "debug.h"
#include "memory.h"

int main(int argc, const char * argv[]) {
    printf("Zhenzhu VM is under development... watch for bugs!\n");

    ZZVM* vm = zzNewVM(NULL);

    printf("=== VM initialized. ===\n");

    int constantLocation = addConstant(vm, NUMBER_VAL(1.2));
    writeChunk(vm, CODE_OP_CONSTANT, 123);
    writeChunk(vm, constantLocation, 123);
    constantLocation = addConstant(vm, NUMBER_VAL(3.4));
    writeChunk(vm, CODE_OP_CONSTANT, 123);
    writeChunk(vm, constantLocation, 123);

    writeChunk(vm, CODE_OP_ADD, 123);

    constantLocation = addConstant(vm, NUMBER_VAL(5.6));
    writeChunk(vm, CODE_OP_CONSTANT, 123);
    writeChunk(vm, constantLocation, 123);

    writeChunk(vm, CODE_OP_DIVIDE, 123);

    writeChunk(vm, CODE_OP_NEGATE, 123);

    printf("=== Math done. ===\n");

    constantLocation = addConstant(vm, OBJ_VAL(copyString("Hello,", 6, vm)));
    writeChunk(vm, CODE_OP_CONSTANT, 123);
    writeChunk(vm, constantLocation, 123);
    constantLocation = addConstant(vm, OBJ_VAL(copyString(" world!", 7, vm)));
    writeChunk(vm, CODE_OP_CONSTANT, 123);
    writeChunk(vm, constantLocation, 123);

    writeChunk(vm, CODE_OP_CONCAT, 123);

    writeChunk(vm, CODE_OP_STORE, 123);
    writeChunk(vm, 2, 123);
    writeChunk(vm, CODE_OP_FORGET, 123);

    writeChunk(vm, CODE_OP_NOP, 123);

    disassembleChunk(vm, "test chunk");

    printf("=== BEGINNING INTERPRETATION. ===\n");

    ObjFiber* fiber = zzNewFiber(vm, vm->block->code.data, NULL, 0);

    zzInterpret(vm, fiber);
    zzFreeVM(vm);

    return 0;
}