#ifndef zhenzhu_object_h
#define zhenzhu_object_h

#include "common.h"
#include "value.h"

#define OBJ_TYPE(value)        (AS_OBJ(value)->type)

#define IS_VAR_FRAME(value)    isObjType(value, OBJ_VAR_FRAME)
#define IS_CALL_FRAME(value)   isObjType(value, OBJ_CALL_FRAME)
#define IS_MARK_FRAME(value)   isObjType(value, OBJ_MARK_FRAME)
#define IS_STRING(value)       isObjType(value, OBJ_STRING)
#define IS_CLOSURE(value)      isObjType(value, OBJ_CLOSURE)
#define IS_OP_CLOSURE(value)   isObjType(value, OBJ_OP_CLOSURE)
#define IS_CONTINUATION(value) isObjType(value, OBJ_CONTINUATION)

#define VAL_AS_VAR_FRAME(value)    ((ObjVarFrame*)AS_OBJ(value))
#define VAL_AS_CALL_FRAME(value)   ((ObjCallFrame*)AS_OBJ(value))
#define VAL_AS_MARK_FRAME(value)   ((ObjMarkFrame*)AS_OBJ(value))
#define VAL_AS_STRING(value)       ((ObjString*)AS_OBJ(value))
#define VAL_AS_CSTRING(value)      (((ObjString*)AS_OBJ(value))->chars)
#define VAL_AS_CLOSURE(value)      ((ObjClosure*)AS_OBJ(value))
#define VAL_AS_OP_CLOSURE(value)   ((ObjOpClosure*)AS_OBJ(value))
#define VAL_AS_CONTINUATION(value) ((ObjContinuation*)AS_OBJ(value))

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

struct ObjClosure {
    Obj obj;
    uint8_t* funcLocation;
    Value* vars;
    int varCount;
};

struct ObjOpClosure {
    ObjClosure closure;
    int argCount;
};

struct ObjContinuation {
    Obj obj;
    uint8_t* resumeLocation;
    Value* savedStack;
    int savedStackCount;
    ObjVarFrame** savedFrames;
    int savedFramesCount;
};

static inline bool isObjType(Value value, ObjType type) {
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

void printObject(Value object);

#endif