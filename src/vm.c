#include <stdio.h>

#include "common.h"
#include "vm.h"

void initVM(VM * vm) {
    
}

void freeVM(VM * vm) {

}

// Dispatcher function to run the current chunk in the given vm.
static InterpretResult run(VM * vm) {
#define READ_BYTE() (*vm->ip++)
#define READ_CONSTANT() (vm->chunk->constants.values[READ_BYTE()])

    for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
        disassembleInstruction()
#endif
        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
            case OP_NOP: {
                return INTERPRET_OK;
            }
            case OP_CONSTANT: {
                Value constant = READ_CONSTANT();
                printValue(constant);
                printf("\n");
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