#ifndef zhenzhu_value_h
#define zhenzhu_value_h

#include "common.h"

typedef double Value;

typedef struct {
    int count;
    int capacity;
    Value * values;
} ValueArray;

void initValueArray(ValueArray * array);
void writeValueArray(ValueArray * array, Value value);
void freeValueArray(ValueArray * array);

// Logs a textual representation of the given value to the output
void printValue(Value value);

#endif