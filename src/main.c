#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "chunk.h"
#include "object.h"
#include "vm.h"
#include "debug.h"

int main(int argc, const char * argv[]) {
    printf("Zhenzhu VM is in progress... \n");

    ZZVM vm;
    initVM(&vm);

    Chunk chunk;
    initChunk(&chunk);

    int constantLocation = addConstant(&chunk, NUMBER_VAL(1.2));
    writeChunk(&chunk, OP_CONSTANT, 123);
    writeChunk(&chunk, constantLocation, 123);
    constantLocation = addConstant(&chunk, NUMBER_VAL(3.4));
    writeChunk(&chunk, OP_CONSTANT, 123);
    writeChunk(&chunk, constantLocation, 123);

    writeChunk(&chunk, OP_ADD, 123);

    constantLocation = addConstant(&chunk, NUMBER_VAL(5.6));
    writeChunk(&chunk, OP_CONSTANT, 123);
    writeChunk(&chunk, constantLocation, 123);

    writeChunk(&chunk, OP_DIVIDE, 123);

    writeChunk(&chunk, OP_NEGATE, 123);

    constantLocation = addConstant(&chunk, OBJ_VAL(copyString("Hello,", 6, &vm)));
    writeChunk(&chunk, OP_CONSTANT, 123);
    writeChunk(&chunk, constantLocation, 123);
    constantLocation = addConstant(&chunk, OBJ_VAL(copyString(" world!", 7, &vm)));
    writeChunk(&chunk, OP_CONSTANT, 123);
    writeChunk(&chunk, constantLocation, 123);

    writeChunk(&chunk, OP_CONCAT, 123);

    writeChunk(&chunk, OP_STORE, 123);
    writeChunk(&chunk, 2, 123);
    writeChunk(&chunk, OP_FORGET, 123);

    writeChunk(&chunk, OP_NOP, 123);

    disassembleChunk(&chunk, "test chunk");
    interpret(&chunk, &vm);
    freeChunk(&chunk);
    freeVM(&vm);

    return 0;
}