// Test the string operations and capabilities of the VM.

BEGIN_TEST("String concatenation")

CONSTANT(OBJ_VAL(copyString("Hello,", 6, vm)))
CONSTANT(OBJ_VAL(copyString(" world!", 7, vm)))

WRITE_INST(CONSTANT, 123)
WRITE_BYTE(0, 123)
WRITE_INST(CONSTANT, 123)
WRITE_BYTE(1, 123)
WRITE_INST(CONCAT, 123)

WRITE_INST(ABORT, 123)
WRITE_BYTE(0, 123)

VERIFY_FRAMES(0)
VERIFY_STACK(1)
VERIFY_STRING("Hello, world!")

END_TEST();