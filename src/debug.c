#include <stdio.h>

#include "debug.h"

// Easy function to print out 1-byte instructions and return the new offset.
static int simpleInstruction(const char * name, int offset) {
    printf("%s\n", name);
    return offset + 1;
}

static int byteArgInstruction(const char* name, ZZVM* vm, int offset) {
    printf("%-16s %d\n", name, vm->block->code.data[offset + 1]);
    return offset + 2;
}

static int shortArgInstruction(const char* name, ZZVM* vm, int offset) {
    uint8_t* code = vm->block->code.data;
    printf("%-16s %d\n", name, (code[offset+1] << 8) | code[offset+2]);
    return offset + 3;
}

static int constantInstruction(const char * name, ZZVM* vm, int offset) {
    uint8_t constant = vm->block->code.data[offset + 1];
    printf("%-16s %4d '", name, constant);
    printValue(vm->block->constants.data[constant]);
    printf("'\n");
    return offset + 2;
}

void disassembleChunk(ZZVM* vm, const char * name) {
    printf("== %s ==\n", name);

    for (int offset = 0; offset < vm->block->code.count;) {
        // recall that instructions can be different size, hence not simply incrementing here
        offset = disassembleInstruction(vm, offset);
    }
}

int disassembleInstruction(ZZVM* vm, int offset) {
    ASSERT(offset < vm->block->lines.count, "No line at the specified offset!");
    ASSERT(offset < vm->block->code.count, "No instruction at the specified offset!");

    printf("%04d ", offset);
    if (offset > 0 && vm->block->lines.data[offset] == vm->block->lines.data[offset - 1]) {
        printf("   | ");
    } else {
        printf("%4d ", vm->block->lines.data[offset]);
    }

    uint8_t instruction = vm->block->code.data[offset];
    switch (instruction) {
        case CODE_NOP:
            return simpleInstruction("NOP", offset);
        case CODE_ABORT:
            return byteArgInstruction("ABORT", vm, offset);
        case CODE_CONSTANT:
            return constantInstruction("CONSTANT", vm, offset);
        case CODE_NEGATE:
            return simpleInstruction("NEGATE", offset);
        case CODE_ADD:
            return simpleInstruction("ADD", offset);
        case CODE_SUBTRACT:
            return simpleInstruction("SUBTRACT", offset);
        case CODE_MULTIPLY:
            return simpleInstruction("MULTIPLY", offset);
        case CODE_DIVIDE:
            return simpleInstruction("DIVIDE", offset);
        case CODE_EQUAL:
            return simpleInstruction("EQUAL", offset);
        case CODE_GREATER:
            return simpleInstruction("GREATER", offset);
        case CODE_LESS:
            return simpleInstruction("LESS", offset);
        case CODE_TRUE:
            return simpleInstruction("TRUE", offset);
        case CODE_FALSE:
            return simpleInstruction("FALSE", offset);
        case CODE_NOT:
            return simpleInstruction("NOT", offset);
        case CODE_CONCAT:
            return simpleInstruction("CONCAT", offset);
        case CODE_STORE:
            return byteArgInstruction("STORE", vm, offset);
        case CODE_FORGET:
            return simpleInstruction("FORGET", offset);
        case CODE_CALL_FOREIGN:
            return shortArgInstruction("CALL_FOREIGN", vm, offset);
        case CODE_CALL:
            return shortArgInstruction("CALL", vm, offset);
        case CODE_TAILCALL:
            return shortArgInstruction("TAILCALL", vm, offset);
        case CODE_OFFSET:
            return shortArgInstruction("OFFSET", vm, offset);
        case CODE_RETURN:
            return simpleInstruction("RETURN", offset);
        default:
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}