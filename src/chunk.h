#ifndef zhenzhu_chunk_h
#define zhenzhu_chunk_h

#include "common.h"

typedef enum {
    // No-op, doesn't do anything besides move on to the next instruction
    OP_NOP
} OpCode;

typedef struct {
    int count;
    int capacity;
    uint8_t * code;
    int * lines;
} Chunk;

// Creates a new empty chunk with capacity 0.
void initChunk(Chunk* chunk);
// Frees the chunk data and resets the chunk metadata.
void freeChunk(Chunk* chunk);
// Appends a byte to the end of the given chunk, growing the capacity of the chunk if required.
void writeChunk(Chunk* chunk, uint8_t byte, int line);

#endif