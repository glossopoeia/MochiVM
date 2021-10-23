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

OPCODE(NOP)
OPCODE(ABORT)

OPCODE(TRUE)
OPCODE(FALSE)
OPCODE(NOT)

OPCODE(CONSTANT)

OPCODE(NEGATE)
OPCODE(ADD)
OPCODE(SUBTRACT)
OPCODE(MULTIPLY)
OPCODE(DIVIDE)
OPCODE(EQUAL)
OPCODE(GREATER)
OPCODE(LESS)

OPCODE(CONCAT)

OPCODE(STORE)
OPCODE(OVERWRITE)
OPCODE(FORGET)

OPCODE(CALL_FOREIGN)

OPCODE(OFFSET)
OPCODE(CALL)
OPCODE(TAILCALL)
OPCODE(CALL_CLOSURE)
OPCODE(TAILCALL_CLOSURE)
OPCODE(RETURN)

OPCODE(CLOSURE)
OPCODE(RECURSIVE)
OPCODE(MUTUAL)
OPCODE(ACTION)

OPCODE(HANDLE)
OPCODE(COMPLETE)
OPCODE(ESCAPE)
OPCODE(REACT)
OPCODE(CALL_CONTINUATION)
OPCODE(TAILCALL_CONTINUATION)