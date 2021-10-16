// This defines the bytecode instructions used by the VM. It does so by invoking
// an OPCODE() macro which is expected to be defined at the point that this is
// included. (See: http://en.wikipedia.org/wiki/X_Macro for more.)
//
// The first argument is the name of the opcode. The second is its "stack
// effect" -- the amount that the op code changes the size of the stack. A
// stack effect of 1 means it pushes a value and the stack grows one larger.
// -2 means it pops two values, etc.
//
// Note that the order of instructions here affects the order of the dispatch
// table in the VM's interpreter loop. That in turn affects caching which
// affects overall performance. Take care to run benchmarks if you change the
// order here.

OPCODE(OP_NOP)
OPCODE(OP_ABORT)

OPCODE(OP_TRUE)
OPCODE(OP_FALSE)
OPCODE(OP_NOT)

OPCODE(OP_CONSTANT)

OPCODE(OP_NEGATE)
OPCODE(OP_ADD)
OPCODE(OP_SUBTRACT)
OPCODE(OP_MULTIPLY)
OPCODE(OP_DIVIDE)
OPCODE(OP_EQUAL)
OPCODE(OP_GREATER)
OPCODE(OP_LESS)

OPCODE(OP_CONCAT)

OPCODE(OP_STORE)
OPCODE(OP_OVERWRITE)
OPCODE(OP_FORGET)

OPCODE(OP_CALL_FOREIGN)