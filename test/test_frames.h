
BEGIN_TEST("Tail call with no frames works.");

CONSTANT(TEST_DOUBLE_VAL(1))

WRITE_INT_INST(TAILCALL, 7, 1)
// This push-constant instruction should get skipped by the tailcall,
// so VERIFY_STACK(0) at the bottom verifies that the call actually moves
// the instruction pointer correctly.
WRITE_INST(CONSTANT, 2)
WRITE_BYTE(0, 2)
WRITE_INST(ABORT, 3)
WRITE_BYTE(0, 3)

VERIFY_FRAMES(0)
VERIFY_STACK(0)

END_TEST()

BEGIN_TEST("Offset with no frames works.");

CONSTANT(TEST_DOUBLE_VAL(1))

WRITE_INST(OFFSET, 1)
WRITE_INT(2, 1);
WRITE_INST(CONSTANT, 2)
WRITE_BYTE(0, 2)
WRITE_INST(ABORT, 3)
WRITE_BYTE(0, 3)

VERIFY_FRAMES(0)
VERIFY_STACK(0)

END_TEST()