#ifndef zhenzhu_value_ptr_union_h
#define zhenzhu_value_ptr_union_h

typedef struct {
    bool isHeap;
    union {
        bool boolean;
        double number;
        Obj * obj;
    } as; 
} Value;

#define IS_OBJ(value)     ((value).isHeap)
#define IS_TINY(value)    ((!(value).isHeap))

#define FALSE_VAL    ((Value){false, {.boolean = false}})
#define TRUE_VAL     ((Value){false, {.boolean = true}})

// Value -> 0 or 1.
#define AS_BOOL(value) ((value).as.boolean)

// Value -> Obj*.
#define AS_OBJ(v) ((v).as.obj)

// Interprets [value] as a [double].
static inline double zzValueToNum(Value value) {
    return value.as.number;
}

// Converts [num] to a [Value].
static inline Value zzNumToValue(double num) {
    Value value;
    value.isHeap = false;
    value.as.number = num;
    return value;
}

// Converts the raw object pointer [obj] to a [Value].
static inline Value zzObjectToValue(Obj* obj) {
    Value value;
    value.isHeap = true;
    value.as.obj = obj;
    return value;
}

#endif