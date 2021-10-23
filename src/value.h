#ifndef mochivm_value_h
#define mochivm_value_h

#include "common.h"
#include "mochivm.h"

#define OBJ_TYPE(value)        (AS_OBJ(value)->type)

// These macros promote a primitive C value to a full Zhenzhu Value. There are
// more defined below that are specific to the various Value representations.
#define BOOL_VAL(boolean)   ((boolean) ? TRUE_VAL : FALSE_VAL)
#define NUMBER_VAL(num)     (mochiNumToValue(num))
#define OBJ_VAL(obj)        (mochiObjectToValue((Obj*)(obj)))

#define IS_FIBER(value)        isObjType(value, OBJ_FIBER)
#define IS_VAR_FRAME(value)    isObjType(value, OBJ_VAR_FRAME)
#define IS_CALL_FRAME(value)   isObjType(value, OBJ_CALL_FRAME)
#define IS_MARK_FRAME(value)   isObjType(value, OBJ_MARK_FRAME)
#define IS_STRING(value)       isObjType(value, OBJ_STRING)
#define IS_CLOSURE(value)      isObjType(value, OBJ_CLOSURE)
#define IS_CONTINUATION(value) isObjType(value, OBJ_CONTINUATION)

// These macros cast a Value to one of the specific object types. These do *not*
// perform any validation, so must only be used after the Value has been
// ensured to be the right type.
// AS_OBJ and AS_BOOL are implementation specific.
#define AS_VAR_FRAME(value)     ((ObjVarFrame*)AS_OBJ(value))
#define AS_CALL_FRAME(value)    ((ObjCallFrame*)AS_OBJ(value))
#define AS_MARK_FRAME(value)    ((ObjMarkFrame*)AS_OBJ(value))
#define AS_CLOSURE(value)       ((ObjClosure*)AS_OBJ(value))
#define AS_CONTINUATION(value)  ((ObjContinuation)AS_OBJ(value))
#define AS_FIBER(v)             ((ObjFiber*)AS_OBJ(v))
#define AS_POINTER(v)           ((ObjCPointer*)AS_OBJ(v))
#define AS_FOREIGN(v)           ((ObjForeign*)AS_OBJ(v))
#define AS_NUMBER(value)        (mochiValueToNum(value))
#define AS_STRING(v)            ((ObjString*)AS_OBJ(v))
#define AS_CSTRING(v)           (AS_STRING(v)->chars)

typedef enum {
    OBJ_CODE_BLOCK,
    OBJ_FIBER,
    OBJ_VAR_FRAME,
    OBJ_CALL_FRAME,
    OBJ_MARK_FRAME,
    OBJ_STRING,
    OBJ_CLOSURE,
    OBJ_CONTINUATION,
    OBJ_FOREIGN,
    OBJ_C_POINTER
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

#if MOCHIVM_POINTER_TAGGING

#include "value_ptr_tagged.h"

#elif MOCHIVM_NAN_TAGGING

#include "value_nan_tagged.h"

#else

#include "value_union.h"

#endif

DECLARE_BUFFER(Byte, uint8_t);
DECLARE_BUFFER(Int, int);
DECLARE_BUFFER(Value, Value);

typedef struct ObjCodeBlock {
    Obj obj;
    ByteBuffer code;
    ValueBuffer constants;
    IntBuffer lines;
} ObjCodeBlock;

typedef struct ObjString {
    Obj obj;
    int length;
    char* chars;
} ObjString;

typedef struct ObjVarFrame {
    Obj obj;
    Value* slots;
    int slotCount;
} ObjVarFrame;

typedef struct ObjCallFrame {
    ObjVarFrame vars;
    uint8_t* afterLocation;
} ObjCallFrame;

typedef struct ObjMarkFrame {
    ObjCallFrame call;
    // The identifier that will be searched for when trying to execute a particular operation. Designed to
    // enable efficiently finding sets of related operations that all get handled by the same handler expression.
    // Trying to execute an operation must specify the 'set' of operations the operation belongs to (markId), then
    // the index within the set of the operation itself (operations[i]).
    int markId;
    Value afterClosure;
    Value* operations;
    int operationCount;
} ObjMarkFrame;

struct ObjFiber {
    Obj obj;
    uint8_t* ip;
    bool isRoot;
    bool isSuspended;

    // Value stack, which all instructions that consume and produce data operate upon.
    Value* valueStack;
    Value* valueStackTop;

    // Frame stack, which variable, function, and continuation instructions operate upon.
    ObjVarFrame** frameStack;
    ObjVarFrame** frameStackTop;

    struct ObjFiber* caller;
};

// Represents a function combined with saved context. Arguments via [paramCount] are used to
// inject values from the stack into the call frame at the call site, rather than at
// closure creation time. [captured] stores the values captured from the frame stack at
// closure creation time. [paramCount] is highly useful for passing state through when using
// closures as action handlers, but also makes for a convenient shortcut to store function
// parameters in the frame stack.
typedef struct ObjClosure {
    Obj obj;
    uint8_t* funcLocation;
    uint8_t paramCount;
    uint16_t capturedCount;
    Value captured[];
} ObjClosure;

typedef struct ObjContinuation {
    Obj obj;
    uint8_t* resumeLocation;
    Value* savedStack;
    int savedStackCount;
    ObjVarFrame** savedFrames;
    int savedFramesCount;
} ObjContinuation;

typedef struct ObjCPointer {
    Obj obj;
    void* pointer;
} ObjCPointer;

typedef struct ObjForeign {
    Obj obj;
    int dataCount;
    uint8_t data[];
} ObjForeign;

static inline bool isObjType(Value value, ObjType type) {
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

// Creates a new fiber object with the values from the given initial stack.
ObjFiber* mochiNewFiber(MochiVM* vm, uint8_t* first, Value* initialStack, int initialStackCount);
void mochiFiberPushValue(ObjFiber* fiber, Value v);
Value mochiFiberPopValue(ObjFiber* fiber);

ObjClosure* mochiNewClosure(MochiVM* vm, uint8_t* body, uint8_t paramCount, uint16_t capturedCount);
void mochiClosureCapture(ObjClosure* closure, int captureIndex, Value value);

ObjCodeBlock* mochiNewCodeBlock(MochiVM* vm);

ObjString* takeString(char* chars, int length, MochiVM* vm);
ObjString* copyString(const char* chars, int length, MochiVM* vm);

ObjVarFrame* newVarFrame(Value* vars, int varCount, MochiVM* vm);
ObjCallFrame* newCallFrame(Value* vars, int varCount, uint8_t* afterLocation, MochiVM* vm);
ObjForeign* mochiNewForeign(MochiVM* vm, size_t size);

ObjCPointer* mochiNewCPointer(MochiVM* vm, void* pointer);

// Logs a textual representation of the given value to the output
void printValue(Value value);
void printObject(Value object);

void mochiFreeObj(MochiVM* vm, Obj* object);

#endif