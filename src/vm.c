#include <stdio.h>
#include <string.h>

#include "common.h"
#include "debug.h"
#include "object.h"
#include "memory.h"
#include "vm.h"

static void resetStack(VM * vm) {
    vm->stackTop = vm->stack;
}

void initVM(VM * vm) {
    resetStack(vm);
}

void freeVM(VM * vm) {

}

//static Value peek(int distance, VM * vm) {
//    return vm->stackTop[-1 - distance];
//}

// Dispatcher function to run the current chunk in the given vm.
static InterpretResult run(VM * vm) {
#define READ_BYTE() (*vm->ip++)
#define READ_CONSTANT() (vm->chunk->constants.values[READ_BYTE()])
#define BINARY_OP(valueType, op) \
    do { \
        double b = AS_NUMBER(pop(vm)); \
        double a = AS_NUMBER(pop(vm)); \
        push(valueType(a op b), vm); \
    } while (false)

    for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
        printf("          ");
        for (Value * slot = vm->stack; slot < vm->stackTop; slot++) {
            printf("[ ");
            printValue(*slot);
            printf(" ]");
        }
        printf("\n");
        disassembleInstruction(vm->chunk, (int)(vm->ip - vm->chunk->code));
#endif
        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
            case OP_NOP: {
                printValue(pop(vm));
                printf("\n");
                return INTERPRET_OK;
            }
            case OP_CONSTANT: {
                Value constant = READ_CONSTANT();
                push(constant, vm);
                break;
            }
            case OP_NEGATE:     push(NUMBER_VAL(-AS_NUMBER(pop(vm))), vm); break;
            case OP_ADD:        BINARY_OP(NUMBER_VAL, +); break;
            case OP_SUBTRACT:   BINARY_OP(NUMBER_VAL, -); break;
            case OP_MULTIPLY:   BINARY_OP(NUMBER_VAL, *); break;
            case OP_DIVIDE:     BINARY_OP(NUMBER_VAL, /); break;
            case OP_EQUAL:      BINARY_OP(BOOL_VAL, ==); break;
            case OP_GREATER:    BINARY_OP(BOOL_VAL, >); break;
            case OP_LESS:       BINARY_OP(BOOL_VAL, <); break;
            case OP_TRUE:       push(BOOL_VAL(true), vm); break;
            case OP_FALSE:      push(BOOL_VAL(false), vm); break;
            case OP_NOT:        push(BOOL_VAL(!AS_BOOL(pop(vm))), vm); break;
            case OP_CONCAT: {
                ObjString* b = AS_STRING(pop(vm));
                ObjString* a = AS_STRING(pop(vm));

                int length = a->length + b->length;
                char* chars = ALLOCATE(char, length + 1);
                memcpy(chars, a->chars, a->length);
                memcpy(chars + a->length, b->chars, b->length);
                chars[length] = '\0';

                ObjString* result = takeString(chars, length);
                push(OBJ_VAL(result), vm);
                break;
            }
        }
    }

#undef READ_BYTE
#undef READ_CONSTANT
}

InterpretResult interpret(Chunk * chunk, VM * vm) {
    vm->chunk = chunk;
    vm->ip = chunk->code;
    return run(vm);
}

void push(Value value, VM * vm) {
    *vm->stackTop = value;
    vm->stackTop++;
}

Value pop(VM * vm) {
    vm->stackTop--;
    return *vm->stackTop;
}