#include "common.h"
#include "vm.h"

void initVM(VM * vm) {
    
}

void freeVM(VM * vm) {

}

// Dispatcher function to run the current chunk in the given vm.
static InterpretResult run(VM * vm) {
#define READ_BYTE() (*vm.ip++)

  for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
    disassembleInstruction()
#endif
    uint8_t instruction;
    switch (instruction = READ_BYTE()) {
      case OP_NOP: {
        return INTERPRET_OK;
      }
    }
  }

#undef READ_BYTE
}

InterpretResult interpret(Chunk * chunk, VM * vm) {
    vm->chunk = chunk;
    vm->ip = chunk->code;
    return run(vm);
}