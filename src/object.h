#ifndef zhenzhu_object_h
#define zhenzhu_object_h

#include "common.h"
#include "value.h"



static inline bool isObjType(Value value, ObjType type) {
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

void printObject(Value object);

#endif