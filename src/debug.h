#ifndef zhenzhu_debug_h
#define zhenzhu_debug_h

#include "chunk.h"

// Prints a disassembly of the given chunk, using the given name as a header.
void disassembleChunk(Chunk* chunk, const char* name);
// Prints a disassembly of the instruction at the offset within the given chunk.
// Returns the offset of the next instruction, since instructions may have 
// differing size.
int disassembleInstruction(Chunk* chunk, int offset);

#endif