#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "chunk.h"
#include "debug.h"

int main(int argc, const char * argv[]) {
    printf("Zhenzhu VM is in progress... \n");

    Chunk chunk;
    initChunk(&chunk);
    writeChunk(&chunk, OP_NOP);

    disassembleChunk(&chunk, "test chunk");
    freeChunk(&chunk);

    return 0;
}