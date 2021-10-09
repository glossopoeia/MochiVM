#ifndef zhenzhu_value_h
#define zhenzhu_value_h

#include "common.h"

// These macros promote a primitive C value to a full Zhenzhu Value. There are
// more defined below that are specific to the various Value representations.
#define BOOL_VAL(boolean)   ((boolean) ? TRUE_VAL : FALSE_VAL)
#define NUMBER_VAL(num)     (zzNumToValue(num))
#define OBJ_VAL(obj)        (zzObjectToValue((Obj*)(obj)))

// These macros cast a Value to one of the specific object types. These do *not*
// perform any validation, so must only be used after the Value has been
// ensured to be the right type.
// AS_OBJ and AS_BOOL are implementation specific.
#define AS_VAR_FRAME(value)     ((ObjVarFrame*)value)
#define AS_CALL_FRAME(value)    ((ObjCallFrame*)value)
#define AS_MARK_FRAME(value)    ((ObjMarkFrame*)value)
#define AS_CLOSURE(value)       ((ObjClosure*)AS_OBJ(value))
#define AS_OP_CLOSURE(value)    ((ObjOpClosure*)value)
#define AS_CONTINUATION(value)  ((ObjContinuation)*value)
#define AS_FIBER(v)             ((ObjFiber*)AS_OBJ(v))
#define AS_FOREIGN(v)           ((ObjForeign*)AS_OBJ(v))
#define AS_NUMBER(value)        (zzValueToNum(value))
#define AS_RANGE(v)             ((ObjRange*)AS_OBJ(v))
#define AS_STRING(v)            ((ObjString*)AS_OBJ(v))
#define AS_CSTRING(v)           (AS_STRING(v)->chars)

typedef enum {
    OBJ_FIBER,
    OBJ_VAR_FRAME,
    OBJ_CALL_FRAME,
    OBJ_MARK_FRAME,
    OBJ_STRING,
    OBJ_CLOSURE,
    OBJ_OP_CLOSURE,
    OBJ_CONTINUATION,
} ObjType;

// Base struct for all heap-allocated object types.
typedef struct Obj Obj;
struct Obj {
    ObjType type;
    // Used during garbage collection
    bool isMarked;

    // All currently allocated objects are maintained in a linked list.
    // This allows all memory to be reachable during garbage collection.
    struct Obj* next;
};

#if ZHENZHU_POINTER_TAGGING

#include "value_ptr_tagged.h"

#elif ZHENZHU_NAN_TAGGING

#include "value_nan_tagged.h"

#else

#include "value_union.h"

#endif

typedef struct ObjString ObjString;
typedef struct ObjVarFrame ObjVarFrame;
typedef struct ObjCallFrame ObjCallFrame;
typedef struct ObjMarkFrame ObjMarkFrame;
typedef struct ObjClosure ObjClosure;
typedef struct ObjOpClosure ObjOpClosure;
typedef struct ObjContinuation ObjContinuation;

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

void freeObject(Obj* object);

#endif