#ifndef zhenzhu_value_nan_tagged_h
#define zhenzhu_value_nan_tagged_h

// An IEEE 754 double-precision float is a 64-bit value with bits laid out like:
//
// 1 Sign bit
// | 11 Exponent bits
// | |          52 Mantissa (i.e. fraction) bits
// | |          |
// S[Exponent-][Mantissa------------------------------------------]
//
// The details of how these are used to represent numbers aren't really
// relevant here as long we don't interfere with them. The important bit is NaN.
//
// An IEEE double can represent a few magical values like NaN ("not a number"),
// Infinity, and -Infinity. A NaN is any value where all exponent bits are set:
//
//  v--NaN bits
// -11111111111----------------------------------------------------
//
// Here, "-" means "doesn't matter". Any bit sequence that matches the above is
// a NaN. With all of those "-", it obvious there are a *lot* of different
// bit patterns that all mean the same thing. NaN tagging takes advantage of
// this. We'll use those available bit patterns to represent things other than
// numbers without giving up any valid numeric values.
//
// NaN values come in two flavors: "signalling" and "quiet". The former are
// intended to halt execution, while the latter just flow through arithmetic
// operations silently. We want the latter. Quiet NaNs are indicated by setting
// the highest mantissa bit:
//
//             v--Highest mantissa bit
// -[NaN      ]1---------------------------------------------------
//
// If all of the NaN bits are set, it's not a number. Otherwise, it is.
// That leaves all of the remaining bits as available for us to play with. We
// stuff a few different kinds of things here: special singleton values like
// "true", "false", and pointers to objects allocated on the heap.
// We'll use the sign bit to distinguish non-double values from pointers. If
// it's set, it's a pointer.
//
// v--Pointer or non-double?
// S[NaN      ]1---------------------------------------------------
//
// For non-double values, we store 32 bit values in the lower bits of the double.
//
// For pointers, we are left with 51 bits of mantissa to store an address.
// That's more than enough room for a 32-bit address. Even 64-bit machines
// only actually use 48 bits for addresses, so we've got plenty. We just stuff
// the address right into the mantissa.
//
// Ta-da, double precision numbers, pointers, and non-double values,
// all stuffed into a single 64-bit sequence. Even better, we don't have to
// do any masking or work to extract number values: they are unmodified. This
// means math on doubles is fast.
typedef uint64_t Value;

// A union to let us reinterpret a double as raw bits and back.
typedef union
{
  uint64_t bits64;
  uint32_t bits32[2];
  double num;
} ZhenzhuDoubleBits;

#define ZHENZHU_DOUBLE_QNAN_POS_MIN_BITS (UINT64_C(0x7FF8000000000000))
#define ZHENZHU_DOUBLE_QNAN_POS_MAX_BITS (UINT64_C(0x7FFFFFFFFFFFFFFF))

#define ZHENZHU_DOUBLE_NAN (zzDoubleFromBits(ZHENZHU_DOUBLE_QNAN_POS_MIN_BITS))

static inline double zzDoubleFromBits(uint64_t bits) {
    ZhenzhuDoubleBits data;
    data.bits64 = bits;
    return data.num;
}

static inline uint64_t zzDoubleToBits(double num) {
    ZhenzhuDoubleBits data;
    data.num = num;
    return data.bits64;
}

// A mask that selects the sign bit.
#define SIGN_BIT ((uint64_t)1 << 63)

// The bits that must be set to indicate a quiet NaN.
#define QNAN ((uint64_t)0x7ffc000000000000)

// An object pointer is a NaN with a set sign bit.
#define IS_OBJ(value)   (((value) & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT))
#define IS_TINY(value)  (!IS_OBJ(value))

#define FALSE_VAL     ((Value)(uint64_t)(QNAN | 0))
#define TRUE_VAL      ((Value)(uint64_t)(QNAN | 1))

// Value -> 0 or 1.
#define AS_BOOL(value) ((value) == TRUE_VAL)

// Value -> Obj*.
#define AS_OBJ(value) ((Obj*)(uintptr_t)((value) & ~(SIGN_BIT | QNAN)))

// Interprets [value] as a [double].
static inline double zzValueToNum(Value value) {
    return zzDoubleFromBits(value);
}

// Converts [num] to a [Value].
static inline Value zzNumToValue(double num) {
    return zzDoubleToBits(num);
}

// Converts the raw object pointer [obj] to a [Value].
static inline Value zzObjectToValue(Obj* obj) {
    // The triple casting is necessary here to satisfy some compilers:
    // 1. (uintptr_t) Convert the pointer to a number of the right size.
    // 2. (uint64_t)  Pad it up to 64 bits in 32-bit builds.
    // 3. Or in the bits to make a tagged NaN.
    // 4. Cast to a typedef'd value.
    return (Value)(SIGN_BIT | QNAN | (uint64_t)(uintptr_t)(obj));
}

#endif