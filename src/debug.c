#include <stdio.h>

#include "debug.h"

// Easy function to print out 1-byte instructions and return the new offset.
static int simpleInstruction(const char * name, int offset) {
    printf("%s\n", name);
    return offset + 1;
}

static int constantInstruction(const char * name, ZZVM* vm, int offset) {
    uint8_t constant = vm->block->code.data[offset + 1];
    printf("%-16s %4d '", name, constant);
    printValue(vm->block->constants.data[constant]);
    printf("'\n");
    return offset + 2;
}

void disassembleChunk(ZZVM* vm, const char * name) {
    ASSERT(vm->block->code.data != NULL, "VM must have valid code before disassembling.");
    ASSERT(vm->block->constants.data != NULL, "VM must have valid constants before disassembling.");
    ASSERT(vm->block->lines.data != NULL, "VM must have valid lines before disassembling.");

    printf("== %s ==\n", name);

    for (int offset = 0; offset < vm->block->code.count;) {
        // recall that instructions can be different size, hence not simply incrementing here
        offset = disassembleInstruction(vm, offset);
    }
}

int disassembleInstruction(ZZVM* vm, int offset) {
    printf("%04d ", offset);
    if (offset > 0 &&
        vm->block->lines.data[offset] == vm->block->lines.data[offset - 1]) {
        printf("   | ");
    } else {
        printf("%4d ", vm->block->lines.data[offset]);
    }

    uint8_t instruction = vm->block->code.data[offset];
    switch (instruction) {
        case CODE_OP_NOP:
            return simpleInstruction("OP_NOP", offset);
        case CODE_OP_ABORT:
            printf("%s %d\n", "OP_ABORT", vm->block->code.data[offset+1]);
            return offset + 2;
        case CODE_OP_CONSTANT:
            return constantInstruction("OP_CONSTANT", vm, offset);
        case CODE_OP_NEGATE:
            return simpleInstruction("OP_NEGATE", offset);
        case CODE_OP_ADD:
            return simpleInstruction("OP_ADD", offset);
        case CODE_OP_SUBTRACT:
            return simpleInstruction("OP_SUBTRACT", offset);
        case CODE_OP_MULTIPLY:
            return simpleInstruction("OP_MULTIPLY", offset);
        case CODE_OP_DIVIDE:
            return simpleInstruction("OP_DIVIDE", offset);
        case CODE_OP_EQUAL:
            return simpleInstruction("OP_EQUAL", offset);
        case CODE_OP_GREATER:
            return simpleInstruction("OP_GREATER", offset);
        case CODE_OP_LESS:
            return simpleInstruction("OP_LESS", offset);
        case CODE_OP_TRUE:
            return simpleInstruction("OP_TRUE", offset);
        case CODE_OP_FALSE:
            return simpleInstruction("OP_FALSE", offset);
        case CODE_OP_NOT:
            return simpleInstruction("OP_NOT", offset);
        case CODE_OP_CONCAT:
            return simpleInstruction("OP_CONCAT", offset);
        case CODE_OP_STORE:
            printf("%s %d\n", "OP_STORE", vm->block->code.data[offset+1]);
            return offset + 2;
        case CODE_OP_FORGET:
            return simpleInstruction("OP_FORGET", offset);
        case CODE_OP_CALL_FOREIGN:
            printf("%s %d\n", "OP_CALL_FOREIGN", vm->block->code.data[offset+1]);
            return offset + 2;
        default:
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}