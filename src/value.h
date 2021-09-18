#ifndef zhenzhu_value_h
#define zhenzhu_value_h

#include "common.h"

typedef enum {
    VAL_BOOL,
    VAL_NUMBER,
} ValueType;

typedef struct {
    ValueType type;
    union {
        bool boolean;
        double number;
    } as; 
} Value;

#define BOOL_VAL(value)   ((Value){VAL_BOOL, {.boolean = value}})
#define NUMBER_VAL(value) ((Value){VAL_NUMBER, {.number = value}})

#define AS_BOOL(value)    ((value).as.boolean)
#define AS_NUMBER(value)  ((value).as.number)

#define IS_BOOL(value)    ((value).type == VAL_BOOL)
#define IS_NUMBER(value)  ((value).type == VAL_NUMBER)

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