#ifndef zhenzhu_chunk_h
#define zhenzhu_chunk_h

#include "common.h"
#include "value.h"

typedef enum {
    // No-op, doesn't do anything besides move on to the next instruction
    OP_NOP,
    OP_TRUE,
    OP_FALSE,
    OP_NOT,
    // Push a constant onto the stack
    OP_CONSTANT,
    OP_NEGATE,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_EQUAL,
    OP_GREATER,
    OP_LESS,
    OP_CONCAT,

    OP_STORE,
    OP_OVERWRITE,
    OP_FORGET,
} OpCode;

#endif