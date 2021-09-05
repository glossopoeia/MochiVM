#ifndef zhenzhu_value_h
#define zhenzhu_value_h

#include "common.h"

typedef enum {
    VAL_BOOL,
    VAL_DOUBLE,
    VAL_REF,
    VAL_LIST,
    VAL_CLOSURE,
    VAL_OPERATION_CLOSURE,
    VAL_CONTINUATION,
    VAL_CONSTRUCTED,
    VAL_RECORD,
    VAL_VARIANT
} ValueType;

struct sValueListNode;
struct sValueList;
struct sValue;

typedef struct sValueListNode {
    struct sValueListNode * prev;
    struct sValueListNode * next;
    struct sValue * value;
}

typedef struct sValueList {
    int count;
    int capacity;
    struct sValue * values;
}

typedef struct sValue {
    ValueType type;
    union {
        bool vBool;
        double vDouble;
        struct sValueList * vList;
    } as;
} Value;

#define IS_BOOL(value) ((value).type == VAL_BOOL)
#define IS_DOUBLE(value) ((value).type == VAL_DOUBLE)
#define IS_LIST(value) ((value).type == VAL_LIST)

#define AS_BOOL(value) ((value).as.vBool)
#define AS_DOUBLE(value) ((value).as.vDouble)

#define BOOL_VAL(value) ((Value){VAL_BOOL, {.vBool = value}})
#define DOUBLE_VAL(value) ((Value){VAL_DOUBLE, {.vDouble = value}})
#define LIST_VAL(value) ((Value){VAL_LIST, {.vList = value}})

#endif