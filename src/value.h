#ifndef zhenzhu_value_h
#define zhenzhu_value_h

#include "common.h"

typedef enum {
    VAL_BOOL,
    VAL_DOUBLE,
    VAL_REF,
    VAL_LIST,
    VAL_VECTOR,
    VAL_SLICE,
    VAL_CLOSURE,
    VAL_OPERATION_CLOSURE,
    VAL_CONTINUATION,
    VAL_CONSTRUCTED,
    VAL_RECORD,
    VAL_VARIANT
} ValueType;

struct sValue;
struct sValueListNode;

typedef struct sValueListNode {
    struct sValueListNode * prev;
    struct sValueListNode * next;
    struct sValue * element;
} ValueListNode;

typedef struct {
    int count;
    int capacity;
    struct sValue * values;
} ValueVector;

typedef struct {
    int offset;
    int count;
    ValueVector * original;
} ValueSlice;

typedef struct sValue {
    ValueType type;
    union {
        bool vBool;
        double vDouble;
        ValueListNode * vList;
        ValueVector * vVector;
        ValueSlice * vSlice;
    } as;
} Value;

#define IS_BOOL(value) ((value).type == VAL_BOOL)
#define IS_DOUBLE(value) ((value).type == VAL_DOUBLE)
#define IS_LIST(value) ((value).type == VAL_LIST)
#define IS_VECTOR(value) ((value).type == VAL_VECTOR)
#define IS_SLICE(value) ((value).type == VAL_SLICE)

#define AS_BOOL(value) ((value).as.vBool)
#define AS_DOUBLE(value) ((value).as.vDouble)
#define AS_LIST(value) ((value).as.vList)
#define AS_VECTOR(value) ((value).as.vVector)
#define AS_SLICE(value) ((value).as.vSlice)

#define BOOL_VAL(value) ((Value){VAL_BOOL, {.vBool = value}})
#define DOUBLE_VAL(value) ((Value){VAL_DOUBLE, {.vDouble = value}})
#define LIST_VAL(value) ((Value){VAL_LIST, {.vList = value}})
#define VECTOR_VAL(value) ((Value){VAL_VECTOR, {.vVector = value}})
#define SLICE_VAL(value) ((Value){VAL_SLICE, {.vSlice = value}})

// Logs a textual representation of the given value to the output
void printValue(Value value);

#endif