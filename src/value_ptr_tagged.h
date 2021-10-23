#ifndef mochivm_value_ptr_tagged_h
#define mochivm_value_ptr_tagged_h

typedef uint64_t Value;

#define PTR_MASK ((uint64_t)0xfffffffffffffffe)
#define TINY_TAG ((uint64_t)1)

#define IS_TINY(value)  (((value) & TINY_TAG) == TINY_TAG)
#define IS_OBJ(value)   (((value) & TINY_TAG) == (uint64_t)0)

#define FALSE_VAL   ((Value)TINY_TAG)
#define TRUE_VAL    ((Value)(uint64_t)(TINY_TAG | 2))

#define AS_BOOL(value)  ((value) == TRUE_VAL)

#define AS_OBJ(value)   ((Obj*)(uintptr_t)((value) & PTR_MASK))

// Interprets [value] as a [double].
static inline double mochiValueToNum(Value value) {
    ASSERT(false, "mochiValueToNum not yet implemented for pointer tagging.");
    return 0;
}

// Converts [num] to a [Value].
static inline Value mochiNumToValue(double num) {
    ASSERT(false, "mochiNumToValue not yet implemented for pointer tagging.");
    return (Value)(uint64_t)0;
}

// Converts the raw object pointer [obj] to a [Value].
static inline Value mochiObjectToValue(Obj* obj) {
    // The triple casting is necessary here to satisfy some compilers:
    // 1. (uintptr_t) Convert the pointer to a number of the right size.
    // 2. (uint64_t)  Pad it up to 64 bits in 32-bit builds.
    // 3. Or in the bits to make a tagged NaN.
    // 4. Cast to a typedef'd value.
    return (Value)(PTR_MASK & (uint64_t)(uintptr_t)(obj));
}

#endif