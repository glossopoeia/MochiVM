#ifndef zhenzhu_object_h
#define zhenzhu_object_h

#include "common.h"
#include "value.h"

#define OBJ_TYPE(value)        (AS_OBJ(value)->type)

#define IS_VAR_FRAME(value)    isObjType(value, OBJ_VAR_FRAME)
#define IS_CALL_FRAME(value)   isObjType(value, OBJ_CALL_FRAME)
#define IS_MARK_FRAME(value)   isObjType(value, OBJ_MARK_FRAME)
#define IS_STRING(value)       isObjType(value, OBJ_STRING)

#define AS_VAR_FRAME(value)    ((ObjVarFrame*)value)
#define AS_CALL_FRAME(value)   ((ObjCallFrame*)value)
#define AS_MARK_FRAME(value)   ((ObjMarkFrame*)value)
#define AS_STRING(value)       ((ObjString*)value)
#define AS_CSTRING(value)      (((ObjString*)value)->chars)

#define VAL_AS_VAR_FRAME(value)    ((ObjVarFrame*)AS_OBJ(value))
#define VAL_AS_CALL_FRAME(value)   ((ObjCallFrame*)AS_OBJ(value))
#define VAL_AS_MARK_FRAME(value)   ((ObjMarkFrame*)AS_OBJ(value))
#define VAL_AS_STRING(value)       ((ObjString*)AS_OBJ(value))
#define VAL_AS_CSTRING(value)      (((ObjString*)AS_OBJ(value))->chars)

typedef enum {
    OBJ_VAR_FRAME,
    OBJ_CALL_FRAME,
    OBJ_MARK_FRAME,
    OBJ_STRING,
} ObjType;

struct Obj {
    ObjType type;
    struct Obj* next;
};

struct ObjString {
    Obj obj;
    int length;
    char* chars;
};

struct ObjVarFrame {
    Obj obj;
    Value* slots;
    int slotCount;
};

struct ObjCallFrame {
    ObjVarFrame vars;
    uint8_t* afterLocation;
};

struct ObjMarkFrame {
    ObjCallFrame call;
    // The identifier that will be searched for when trying to execute a particular operation. Designed to
    // enable efficiently finding sets of related operations that all get handled by the same handler expression.
    // Trying to execute an operation must specify the 'set' of operations the operation belongs to (markId), then
    // the index within the set of the operation itself (operations[i]).
    int markId;
    Value afterClosure;
    Value* operations;
    int operationCount;
};

static inline bool isObjType(Value value, ObjType type) {
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

void printObject(Obj* object);

#endif