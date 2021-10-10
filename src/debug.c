#include <stdio.h>

#include "debug.h"

// Easy function to print out 1-byte instructions and return the new offset.
static int simpleInstruction(const char * name, int offset) {
    printf("%s\n", name);
    return offset + 1;
}

static int constantInstruction(const char * name, ZZVM* vm, int offset) {
    uint8_t constant = vm->code->data[offset + 1];
    printf("%-16s %4d '", name, constant);
    printValue(vm->constants->data[constant]);
    printf("'\n");
    return offset + 2;
}

void disassembleChunk(ZZVM* vm, const char * name) {
    printf("== %s ==\n", name);

    for (int offset = 0; offset < vm->code->count;) {
        // recall that instructions can be different size, hence not simply incrementing here
        offset = disassembleInstruction(vm, offset);
    }
}

int disassembleInstruction(ZZVM* vm, int offset) {
    printf("%04d ", offset);
    if (offset > 0 &&
        vm->lines->data[offset] == vm->lines->data[offset - 1]) {
        printf("   | ");
    } else {
        printf("%4d ", vm->lines->data[offset]);
    }

    uint8_t instruction = vm->code->data[offset];
    switch (instruction) {
        case OP_NOP:
            return simpleInstruction("OP_NOP", offset);
        case OP_CONSTANT:
            return constantInstruction("OP_CONSTANT", vm, offset);
        case OP_NEGATE:
            return simpleInstruction("OP_NEGATE", offset);
        case OP_ADD:
            return simpleInstruction("OP_ADD", offset);
        case OP_SUBTRACT:
            return simpleInstruction("OP_SUBTRACT", offset);
        case OP_MULTIPLY:
            return simpleInstruction("OP_MULTIPLY", offset);
        case OP_DIVIDE:
            return simpleInstruction("OP_DIVIDE", offset);
        case OP_EQUAL:
            return simpleInstruction("OP_EQUAL", offset);
        case OP_GREATER:
            return simpleInstruction("OP_GREATER", offset);
        case OP_LESS:
            return simpleInstruction("OP_LESS", offset);
        case OP_TRUE:
            return simpleInstruction("OP_TRUE", offset);
        case OP_FALSE:
            return simpleInstruction("OP_FALSE", offset);
        case OP_NOT:
            return simpleInstruction("OP_NOT", offset);
        case OP_CONCAT:
            return simpleInstruction("OP_CONCAT", offset);
        case OP_STORE:
            printf("%s %d\n", "OP_STORE", vm->code->data[offset+1]);
            return offset + 2;
        case OP_FORGET:
            return simpleInstruction("OP_FORGET", offset);
        default:
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}