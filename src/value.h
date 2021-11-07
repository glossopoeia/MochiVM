#ifndef mochivm_value_h
#define mochivm_value_h

#include <string.h>

#include "common.h"
#include "mochivm.h"

#define ASSERT_OBJ_TYPE(obj, objType, message)  ASSERT(((Obj*)obj)->type == objType, message)
#define OBJ_ARRAY_COPY(objDest, objSrc, count)  memcpy((Obj**)(objDest), (Obj**)(objSrc), sizeof(Obj*) * (count))

#define OBJ_TYPE(value)        (AS_OBJ(value)->type)

// These macros promote a primitive C value to a full Zhenzhu Value. There are
// more defined below that are specific to the various Value representations.
#define BOOL_VAL(boolean)   ((boolean) ? TRUE_VAL : FALSE_VAL)
#define NUMBER_VAL(num)     (mochiNumToValue(num))
#define OBJ_VAL(obj)        (mochiObjectToValue((Obj*)(obj)))

#define IS_FIBER(value)        isObjType(value, OBJ_FIBER)
#define IS_VAR_FRAME(value)    isObjType(value, OBJ_VAR_FRAME)
#define IS_CALL_FRAME(value)   isObjType(value, OBJ_CALL_FRAME)
#define IS_HANDLE_FRAME(value) isObjType(value, OBJ_HANDLE_FRAME)
#define IS_STRING(value)       isObjType(value, OBJ_STRING)
#define IS_CLOSURE(value)      isObjType(value, OBJ_CLOSURE)
#define IS_CONTINUATION(value) isObjType(value, OBJ_CONTINUATION)

// These macros cast a Value to one of the specific object types. These do *not*
// perform any validation, so must only be used after the Value has been
// ensured to be the right type.
// AS_OBJ and AS_BOOL are implementation specific.
#define AS_VAR_FRAME(value)     ((ObjVarFrame*)AS_OBJ(value))
#define AS_CALL_FRAME(value)    ((ObjCallFrame*)AS_OBJ(value))
#define AS_HANDLE_FRAME(value)  ((ObjHandleFrame*)AS_OBJ(value))
#define AS_CLOSURE(value)       ((ObjClosure*)AS_OBJ(value))
#define AS_CONTINUATION(value)  ((ObjContinuation*)AS_OBJ(value))
#define AS_FIBER(v)             ((ObjFiber*)AS_OBJ(v))
#define AS_POINTER(v)           ((ObjCPointer*)AS_OBJ(v))
#define AS_FOREIGN(v)           ((ObjForeign*)AS_OBJ(v))
#define AS_LIST(v)              ((ObjList*)AS_OBJ(v))
#define AS_ARRAY(v)             ((ObjArray*)AS_OBJ(v))
#define AS_SLICE(v)             ((ObjSlice*)AS_OBJ(v))
#define AS_NUMBER(value)        (mochiValueToNum(value))
#define AS_STRING(v)            ((ObjString*)AS_OBJ(v))
#define AS_CSTRING(v)           (AS_STRING(v)->chars)

typedef enum {
    OBJ_LIST,
    OBJ_CODE_BLOCK,
    OBJ_FIBER,
    OBJ_VAR_FRAME,
    OBJ_CALL_FRAME,
    OBJ_HANDLE_FRAME,
    OBJ_STRING,
    OBJ_CLOSURE,
    OBJ_CONTINUATION,
    OBJ_FOREIGN,
    OBJ_C_POINTER,
    OBJ_FOREIGN_RESUME,
    OBJ_ARRAY,
    OBJ_SLICE
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
    IntBuffer labelIndices;
    ValueBuffer labels;
} ObjCodeBlock;

typedef struct ObjString {
    Obj obj;
    int length;
    char* chars;
} ObjString;

// This enum provides a way for compiler writers to specify that some closures-as-handlers have
// certain assumptions guaranteed that allow more efficient operation. For instance, RESUME_NONE
// will prevent a handler closure from capturing the continuation, since it is never resumed anyway,
// saving a potentially large allocation and copy. RESUME_ONCE_TAIL treats a handler closure call
// just like any other closure call. The most general option, but the least efficient, is RESUME_MANY,
// which can be thought of as the default for handler closures. The default for all closures is
// RESUME_MANY, even those which are never used as handlers, because continuation saving is only done
// during the ESCAPE instruction and so RESUME_MANY is never acted upon for the majority of closures.
typedef enum {
    RESUME_NONE,
    RESUME_ONCE,
    RESUME_ONCE_TAIL,
    RESUME_MANY
} ResumeLimit;

// Represents a function combined with saved context. Arguments via [paramCount] are used to
// inject values from the stack into the call frame at the call site, rather than at
// closure creation time. [captured] stores the values captured from the frame stack at
// closure creation time. [paramCount] is highly useful for passing state through when using
// closures as action handlers, but also makes for a convenient shortcut to store function
// parameters in the frame stack. [resumeLimit] is a way for the runtime to specify how many times
// a closure-as-handler can resume in a handle context.
typedef struct ObjClosure {
    Obj obj;
    uint8_t* funcLocation;
    uint8_t paramCount;
    uint16_t capturedCount;
    ResumeLimit resumeLimit;
    Value captured[];
} ObjClosure;

typedef struct ObjVarFrame {
    Obj obj;
    Value* slots;
    int slotCount;
} ObjVarFrame;

typedef struct ObjCallFrame {
    ObjVarFrame vars;
    uint8_t* afterLocation;
} ObjCallFrame;

typedef struct ObjHandleFrame {
    ObjCallFrame call;
    // The identifier that will be searched for when trying to execute a particular operation. Designed to
    // enable efficiently finding sets of related handlers that all get handled by the same handler expression.
    // Trying to execute an operation must specify the 'set' of handlers the operation belongs to (handleId), then
    // the index within the set of the operation itself (handlers[i]).
    int handleId;
    int nesting;
    ObjClosure* afterClosure;
    ObjClosure** handlers;
    uint8_t handlerCount;
} ObjHandleFrame;

struct ObjFiber {
    Obj obj;
    uint8_t* ip;
    bool isRoot;
    bool isSuspended;

    // Value stack, upon which all instructions that consume and produce data operate.
    Value* valueStack;
    Value* valueStackTop;

    // Frame stack, upon which variable, function, and continuation instructions operate.
    ObjVarFrame** frameStack;
    ObjVarFrame** frameStackTop;

    // Root stack, a smaller Object stack used to temporarily store data so it doesn't get GC'ed.
    Obj** rootStack;
    Obj** rootStackTop;

    struct ObjFiber* caller;
};

typedef struct ObjContinuation {
    Obj obj;
    uint8_t* resumeLocation;
    uint8_t paramCount;
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

// A C-function which takes a Mochi closure as a callback is tricky to implement. This structure is
// also passed in where closures are expected so that the C-callback which calls the Mochi callback
// can remember where it was to call the Mochi callback properly.
typedef struct ForeignResume {
    Obj obj;
    MochiVM* vm;
    ObjFiber* fiber;
} ForeignResume;

typedef struct ObjList {
    Obj obj;
    Value elem;
    struct ObjList* next;
} ObjList;

typedef struct ObjArray {
    Obj obj;
    ValueBuffer elems;
} ObjArray;

typedef struct ObjSlice {
    Obj obj;
    int start;
    int count;
    ObjArray* source;
} ObjSlice;

static inline bool isObjType(Value value, ObjType type) {
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

static inline void valueArrayCopy(Value* dest, Value* src, int count) {
    memcpy(dest, src, sizeof(Value) * count);
}

// Creates a new fiber object with the values from the given initial stack.
ObjFiber* mochiNewFiber(MochiVM* vm, uint8_t* first, Value* initialStack, int initialStackCount);
void mochiFiberPushValue(ObjFiber* fiber, Value v);
Value mochiFiberPopValue(ObjFiber* fiber);
void mochiFiberPushFrame(ObjFiber* fiber, ObjVarFrame* frame);
ObjVarFrame* mochiFiberPopFrame(ObjFiber* fiber);
void mochiFiberPushRoot(ObjFiber* fiber, Obj* root);
Obj* mochiFiberPopRoot(ObjFiber* fiber);

ObjClosure* mochiNewClosure(MochiVM* vm, uint8_t* body, uint8_t paramCount, uint16_t capturedCount);
void mochiClosureCapture(ObjClosure* closure, int captureIndex, Value value);

ObjContinuation* mochiNewContinuation(MochiVM* vm, uint8_t* resume, uint8_t paramCount, int savedStack, int savedFrames);

ObjCodeBlock* mochiNewCodeBlock(MochiVM* vm);

ObjString* takeString(char* chars, int length, MochiVM* vm);
ObjString* copyString(const char* chars, int length, MochiVM* vm);

ObjVarFrame* newVarFrame(Value* vars, int varCount, MochiVM* vm);
ObjCallFrame* newCallFrame(Value* vars, int varCount, uint8_t* afterLocation, MochiVM* vm);
ObjHandleFrame* mochinewHandleFrame(MochiVM* vm, int handleId, uint8_t paramCount, uint8_t handlerCount, uint8_t* after);

ObjForeign* mochiNewForeign(MochiVM* vm, size_t size);
ObjCPointer* mochiNewCPointer(MochiVM* vm, void* pointer);
ForeignResume* mochiNewResume(MochiVM* vm, ObjFiber* fiber);

ObjList* mochiListNil(MochiVM* vm);
ObjList* mochiListCons(MochiVM* vm, Value elem, ObjList* tail);
ObjList* mochiListTail(ObjList* list);
Value mochiListHead(ObjList* list);
int mochiListLength(ObjList* list);

ObjArray* mochiArrayNil(MochiVM* vm);
ObjArray* mochiArrayFill(MochiVM* vm, int amount, Value elem, ObjArray* array);
ObjArray* mochiArraySnoc(MochiVM* vm, Value elem, ObjArray* array);
Value mochiArrayGetAt(MochiVM* vm, int index, ObjArray* array);
void mochiArraySetAt(MochiVM* vm, int index, Value value, ObjArray* array);
int mochiArrayLength(MochiVM* vm, ObjArray* array);
ObjArray* mochiArrayCopy(MochiVM* vm, int start, int length, ObjArray* array);

ObjSlice* mochiArraySlice(MochiVM* vm, int start, int length, ObjArray* array);
ObjSlice* mochiSubslice(MochiVM* vm, int start, int length, ObjSlice* slice);
Value mochiSliceGetAt(MochiVM* vm, int index, ObjSlice* slice);
void mochiSliceSetAt(MochiVM* vm, int index, Value vlaue, ObjSlice* slice);
int mochiSliceLength(MochiVM* vm, ObjSlice* slice);
ObjArray* mochiSliceCopy(MochiVM* vm, ObjSlice* slice);

// Logs a textual representation of the given value to the output
void printValue(MochiVM* vm, Value value);
void printObject(MochiVM* vm, Value object);

void mochiFreeObj(MochiVM* vm, Obj* object);

#endif