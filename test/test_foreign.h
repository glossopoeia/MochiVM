// Test the foreign function capabilities of the VM

BEGIN_TEST("LibUV foreign function tests")

WRITE_INST(CALL_FOREIGN, 123)
WRITE_BYTE(0, 123)
WRITE_BYTE(0, 123)
WRITE_INST(CALL_FOREIGN, 124)
WRITE_BYTE(0, 124)
WRITE_BYTE(1, 124)
WRITE_INST(ABORT, 103)
WRITE_BYTE(0, 103)

VERIFY_FRAMES(0)
VERIFY_STACK(0)

END_TEST();