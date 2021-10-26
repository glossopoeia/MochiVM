#include <stdio.h>
#include <string.h>

#include "common.h"
#include "debug.h"
#include "memory.h"
#include "vm.h"

#if MOCHIVM_BATTERY_UV
    #include "uv.h"
    #include "battery_uv.h"
#endif

// Generic function to create a call frame from a closure based on some data known about it. Can supply a var frame
// that will be spliced between the parameters and the captured values, but if this isn't needed, supply NULL for it.
// Modifies the fiber stack, and expects the parameters to be in correct order at the top of the stack.
static ObjCallFrame* callClosureFrame(MochiVM* vm, ObjFiber* fiber, ObjClosure* capture, ObjVarFrame* frameVars, ObjContinuation* cont, uint8_t* after) {
    ASSERT((fiber->valueStackTop - fiber->valueStack) >= capture->paramCount, "callClosureFrame: Not enough values on the value stack to call the closure.");

    int varCount = (cont != NULL ? 1 : 0) + capture->paramCount + capture->capturedCount + (frameVars != NULL ? frameVars->slotCount : 0);
    Value* vars = ALLOCATE_ARRAY(vm, Value, varCount);

    int offset = 0;
    if (cont != NULL) {
        vars[0] = OBJ_VAL(cont);
        offset += 1;
    }

    for (int i = 0; i < capture->paramCount; i++) {
        vars[offset + i] = *(--fiber->valueStackTop);
    }
    offset += capture->paramCount;

    if (frameVars != NULL) {
        valueArrayCopy(vars + offset, frameVars->slots, frameVars->slotCount);
        offset += frameVars->slotCount;
    }
    valueArrayCopy(vars + offset, capture->captured, capture->capturedCount);
    return newCallFrame(vars, varCount, after, vm);
}

// Walk the frame stack backwards looking for a mark frame with the given mark id that is 'unnested',
// i.e. with a nesting level of 0. Injecting increases the nesting levels of the nearest mark frames with
// a given mark id, while ejecting decreases the nesting level. This dual functionality allows some
// actions to be handled by handlers 'containing' inner handlers that would otherwise have handled the action.
// This function drives the actual effect of the nesting by continuing to walk down mark frames even if a
// mark frame with the requested id is found if it is 'nested', i.e. with a nesting level greater than 0.
static int findFreeHandler(ObjFiber* fiber, int markId) {
    int stackCount = fiber->frameStackTop - fiber->frameStack;
    int index = 0;
    for (; index < stackCount; index++) {
        ObjVarFrame* frame = *(fiber->frameStackTop - index - 1);
        if (frame->obj.type != OBJ_MARK_FRAME) {
            continue;
        }
        ObjMarkFrame* mark = (ObjMarkFrame*)frame;
        if (mark->markId == markId && mark->nesting == 0) {
            break;
        }
    }
    ASSERT(index < stackCount, "Could not find an unnested mark frame with the desired identifier.");
    return index;
}

// Dispatcher function to run a particular fiber in the context of the given vm.
static MochiVMInterpretResult run(MochiVM * vm, register ObjFiber* fiber) {

    // Remember the current fiber in case of GC.
    vm->fiber = fiber;
    fiber->isRoot = true;

    register uint8_t* ip = fiber->ip;
    register uint8_t* codeStart = vm->block->code.data;

#define FROM_START(offset)  (codeStart + (int)(offset))

#define PUSH_VAL(value)     (*fiber->valueStackTop++ = value)
#define POP_VAL()           (*(--fiber->valueStackTop))
#define DROP_VALS(count)    (fiber->valueStackTop = fiber->valueStackTop - (count))
#define PEEK_VAL(index)     (*(fiber->valueStackTop - (index)))
#define VALUE_COUNT()       (fiber->valueStackTop - fiber->valueStack)

#define PUSH_FRAME(frame)   (*fiber->frameStackTop++ = (ObjVarFrame*)(frame))
#define POP_FRAME()         (*(--fiber->frameStackTop))
#define DROP_FRAMES(count)  (fiber->frameStackTop = fiber->frameStackTop - (count))
#define PEEK_FRAME(index)   (*(fiber->frameStackTop - (index)))
#define FRAME_COUNT()       (fiber->frameStackTop - fiber->frameStack)
#define FIND(frame, slot)   ((*(fiber->frameStackTop - 1 - (frame)))->slots[(slot)])

#define READ_BYTE() (*ip++)
#define READ_SHORT() (ip += 2, (int16_t)((ip[-2] << 8) | ip[-1]))
#define READ_USHORT() (ip += 2, (uint16_t)((ip[-2] << 8) | ip[-1]))
#define READ_UINT() (ip += 4, (uint32_t)((ip[-4] << 24) | (ip[-3] << 16) | (ip[-2] << 8) | ip[-1]))
#define READ_CONSTANT() (vm->block->constants.data[READ_BYTE()])
#define BINARY_OP(valueType, op) \
    do { \
        double a = AS_NUMBER(POP_VAL()); \
        double b = AS_NUMBER(POP_VAL()); \
        PUSH_VAL(valueType(a op b)); \
    } while (false)

#ifdef MOCHIVM_DEBUG_TRACE_EXECUTION
    #define DEBUG_TRACE_INSTRUCTIONS() \
        do { \
            disassembleInstruction(vm, (int)(ip - codeStart)); \
            printf("STACK:    "); \
            if (fiber->valueStack >= fiber->valueStackTop) { printf("<empty>"); } \
            for (Value * slot = fiber->valueStack; slot < fiber->valueStackTop; slot++) { \
                printf("[ "); \
                printValue(vm, *slot); \
                printf(" ]"); \
            } \
            printf("\n"); \
            printf("FRAMES:   "); \
            if (fiber->frameStack >= fiber->frameStackTop) { printf("<empty>"); } \
            for (ObjVarFrame** frame = fiber->frameStack; frame < fiber->frameStackTop; frame++) { \
                printf("[ "); \
                printObject(vm, OBJ_VAL(*frame)); \
                printf(" ]"); \
            } \
            printf("\n"); \
        } while (false)
#else
    #define DEBUG_TRACE_INSTRUCTIONS() do { } while (false)
#endif

#if MOCHIVM_BATTERY_UV
    #define UV_EVENT_LOOP()                                                  \
        do {                                                                 \
            printf("Checking for LibUV events.\n");                          \
            uv_run(uv_default_loop(), UV_RUN_NOWAIT);                        \
        } while (false)
#else
    #define UV_EVENT_LOOP() do { } while (false)
#endif

#if MOCHIVM_COMPUTED_GOTO

    static void* dispatchTable[] = {
        #define OPCODE(name) &&code_##name,
        #include "opcodes.h"
        #undef OPCODE
    };

    #define INTERPRET_LOOP    DISPATCH();
    #define CASE_CODE(name)   code_##name

    #define DISPATCH()                                                           \
        do                                                                       \
        {                                                                        \
            DEBUG_TRACE_INSTRUCTIONS();                                          \
            UV_EVENT_LOOP();                                                     \
            if (fiber->isSuspended) { goto CASE_CODE(NOP); }                  \
            goto *dispatchTable[instruction = (Code)READ_BYTE()];                \
        } while (false)

#else

    #define INTERPRET_LOOP                                                       \
        loop:                                                                    \
            DEBUG_TRACE_INSTRUCTIONS();                                          \
            UV_EVENT_LOOP();                                                     \
            if (fiber->isSuspended) { goto loop; }                               \
            switch (instruction = (Code)READ_BYTE())

    #define CASE_CODE(name)  case CODE_##name
    #define DISPATCH()       goto loop

#endif

    Code instruction;
    INTERPRET_LOOP
    {
        CASE_CODE(NOP): {
            DISPATCH();
        }
        CASE_CODE(ABORT): {
            uint8_t retCode = READ_BYTE();
            return retCode;
        }
        CASE_CODE(CONSTANT): {
            Value constant = READ_CONSTANT();
            PUSH_VAL(constant);
            DISPATCH();
        }
        CASE_CODE(NEGATE): {
            double n = AS_NUMBER(POP_VAL());
            PUSH_VAL(NUMBER_VAL(-n));
            DISPATCH();
        }
        CASE_CODE(ADD):      BINARY_OP(NUMBER_VAL, +); DISPATCH();
        CASE_CODE(SUBTRACT): BINARY_OP(NUMBER_VAL, -); DISPATCH();
        CASE_CODE(MULTIPLY): BINARY_OP(NUMBER_VAL, *); DISPATCH();
        CASE_CODE(DIVIDE):   BINARY_OP(NUMBER_VAL, /); DISPATCH();
        CASE_CODE(EQUAL):    BINARY_OP(BOOL_VAL, ==); DISPATCH();
        CASE_CODE(GREATER):  BINARY_OP(BOOL_VAL, >); DISPATCH();
        CASE_CODE(LESS):     BINARY_OP(BOOL_VAL, <); DISPATCH();
        CASE_CODE(TRUE):     PUSH_VAL(BOOL_VAL(true)); DISPATCH();
        CASE_CODE(FALSE):    PUSH_VAL(BOOL_VAL(false)); DISPATCH();
        CASE_CODE(NOT): {
            bool b = AS_BOOL(POP_VAL());
            PUSH_VAL(BOOL_VAL(!b));
            DISPATCH();
        }
        CASE_CODE(CONCAT): {
            ObjString* b = AS_STRING(PEEK_VAL(1));
            ObjString* a = AS_STRING(PEEK_VAL(2));

            int length = a->length + b->length;
            char* chars = ALLOCATE_ARRAY(vm, char, length + 1);
            memcpy(chars, a->chars, a->length);
            memcpy(chars + a->length, b->chars, b->length);
            chars[length] = '\0';
            DROP_VALS(2);

            ObjString* result = takeString(chars, length, vm);
            PUSH_VAL(OBJ_VAL(result));
            DISPATCH();
        }
        CASE_CODE(STORE): {
            uint8_t varCount = READ_BYTE();
            ASSERT(VALUE_COUNT() >= varCount, "Not enough values to store in frame in STORE");

            Value* vars = ALLOCATE_ARRAY(vm, Value, varCount);
            for (int i = 0; i < (int)varCount; i++) {
                vars[i] = *(fiber->valueStackTop - i);
            }

            ObjVarFrame* frame = newVarFrame(vars, varCount, vm);
            PUSH_FRAME(frame);

            DROP_VALS(varCount);
            DISPATCH();
        }
        CASE_CODE(FIND): {
            uint16_t frameIdx = READ_USHORT();
            uint16_t slotIdx = READ_USHORT();

            ASSERT(FRAME_COUNT() > frameIdx, "FIND tried to access a frame outside the bounds of the frame stack.");
            ObjVarFrame* frame = PEEK_FRAME(frameIdx + 1);
            PUSH_VAL(frame->slots[slotIdx]);
            DISPATCH();
        }
        CASE_CODE(OVERWRITE): {
            ASSERT(false, "OVERWRITE not yet implemented.");
            DISPATCH();
        }
        CASE_CODE(FORGET): {
            ASSERT(FRAME_COUNT() > 0, "FORGET expects at least one frame on the frame stack.");
            DROP_FRAMES(1);
            DISPATCH();
        }

        CASE_CODE(CALL_FOREIGN): {
            int16_t fnIndex = READ_SHORT();
            ASSERT(vm->foreignFns.count > fnIndex, "CALL_FOREIGN attempted to address a method outside the bounds of the foreign function collection.");
            MochiVMForeignMethodFn fn = vm->foreignFns.data[fnIndex];
            fn(vm, fiber);
            DISPATCH();
        }
        CASE_CODE(CALL): {
            uint8_t* callPtr = FROM_START(READ_UINT());
            ObjCallFrame* frame = newCallFrame(NULL, 0, ip, vm);
            PUSH_FRAME((ObjVarFrame*)frame);
            ip = callPtr;
            DISPATCH();
        }
        CASE_CODE(TAILCALL): {
            ip = FROM_START(READ_UINT());
            DISPATCH();
        }
        CASE_CODE(CALL_CLOSURE): {
            ASSERT(FRAME_COUNT() > 0, "CALL_CLOSURE requires at least one frame on the frame stack.");
            ASSERT(VALUE_COUNT() > 0, "CALL_CLOSURE requires at least one value on the value stack.");

            // only peek the closure to make sure it gets traced by garbage collection
            ObjClosure* closure = (ObjClosure*)POP_VAL();
            uint8_t* next = closure->funcLocation;

            // need to populate the frame with the captured values, but also the parameters from the stack
            // top of the stack is first in the frame, next is second, etc.
            // captured are copied as they appear in the closure
            mochiPushRoot(vm, (Obj*)closure);
            ObjCallFrame* frame = callClosureFrame(vm, fiber, closure, NULL, NULL, ip);
            mochiPopRoot(vm);

            // jump to the closure body, drop the closure and now-stored stack values, and push the frame
            ip = next;
            PUSH_FRAME(frame);
            DISPATCH();
        }
        CASE_CODE(TAILCALL_CLOSURE): {
            ASSERT(FRAME_COUNT() > 0, "TAILCALL_CLOSURE requires at least one frame on the frame stack.");
            ASSERT(VALUE_COUNT() > 0, "TAILCALL_CLOSURE requires at least one value on the value stack.");

            // only peek the closure to make sure it gets traced by garbage collection
            ObjClosure* closure = (ObjClosure*)POP_VAL();
            uint8_t* next = closure->funcLocation;

            // pop the old frame and create a new frame with the new array of stored values but the same return location
            ObjCallFrame* oldFrame = (ObjCallFrame*)PEEK_FRAME(1);
            mochiPushRoot(vm, (Obj*)closure);
            ObjCallFrame* frame = callClosureFrame(vm, fiber, closure, NULL, NULL, oldFrame->afterLocation);
            mochiPopRoot(vm);

            // jump to the closure body, drop the old frame, drop the closure and the now-stored stack values, and push the new frame
            ip = next;
            DROP_FRAMES(1);
            PUSH_FRAME(frame);
            DISPATCH();
        }
        CASE_CODE(OFFSET): {
            ip = ip + (int)READ_SHORT();
            DISPATCH();
        }
        CASE_CODE(RETURN): {
            ASSERT(FRAME_COUNT() > 0, "RETURN expects at least one frame on the stack.");
            ObjCallFrame* frame = (ObjCallFrame*)POP_FRAME();
            ASSERT_OBJ_TYPE(frame, OBJ_CALL_FRAME, "RETURN expects a frame of type 'call frame' on the frame stack.");
            ip = frame->afterLocation;
            DISPATCH();
        }
        CASE_CODE(CLOSURE): {
            uint8_t* bodyLocation = FROM_START(READ_UINT());
            uint8_t paramCount = READ_BYTE();
            uint16_t closedCount = READ_USHORT();
            ASSERT(paramCount + closedCount <= MOCHIVM_MAX_CALL_FRAME_SLOTS, "Attempt to create closure with more slots than available.");

            ObjClosure* closure = mochiNewClosure(vm, bodyLocation, paramCount, closedCount);
            for (int i = 0; i < closedCount; i++) {
                uint16_t frame = READ_USHORT();
                uint16_t slot = READ_USHORT();
                mochiClosureCapture(closure, i, FIND(frame, slot));
            }
            PUSH_VAL(OBJ_VAL(closure));
            DISPATCH();
        }
        CASE_CODE(RECURSIVE): {
            uint8_t* bodyLocation = FROM_START(READ_UINT());
            uint8_t paramCount = READ_BYTE();
            uint16_t closedCount = READ_USHORT();
            ASSERT(paramCount + closedCount + 1 <= MOCHIVM_MAX_CALL_FRAME_SLOTS, "Attempt to create recursive closure with more slots than available.");

            // add one to closed count to save a slot for the closure itself
            ObjClosure* closure = mochiNewClosure(vm, bodyLocation, paramCount, closedCount + 1);
            // capture everything listed in the instruction args, saving the first spot for the closure itself
            mochiClosureCapture(closure, 0, OBJ_VAL(closure));
            for (int i = 0; i < closedCount; i++) {
                uint16_t frame = READ_USHORT();
                uint16_t slot = READ_USHORT();
                mochiClosureCapture(closure, i + 1, FIND(frame, slot));
            }
            PUSH_VAL(OBJ_VAL(closure));
            DISPATCH();
        }
        CASE_CODE(MUTUAL): {
            uint8_t mutualCount = READ_BYTE();
            ASSERT(VALUE_COUNT() >= mutualCount, "MUTUAL closures attempted to be created with fewer than requested on the value stack.");

            // for each soon-to-be mutually referenced closure,
            // make a new closure with room for references to
            // the other closures and itself
            for (int i = 0; i < mutualCount; i++) {
                ObjClosure* old = AS_CLOSURE(PEEK_VAL(mutualCount - i));
                ObjClosure* closure = mochiNewClosure(vm, old->funcLocation, old->paramCount, old->capturedCount + mutualCount);
                valueArrayCopy(old->captured, closure->captured + mutualCount, old->capturedCount);
                // replace the old closure with the new one
                PEEK_VAL(mutualCount - i) = OBJ_VAL((Obj*)closure);
            }

            // finally, make the closures all reference each other in the same order
            for (int i = 0; i < mutualCount; i++) {
                ObjClosure* closure = AS_CLOSURE(PEEK_VAL(mutualCount - i));
                valueArrayCopy(fiber->valueStackTop - mutualCount, closure->captured, mutualCount);
            }

            DISPATCH();
        }

        CASE_CODE(HANDLE): {
            uint16_t afterOffset = READ_SHORT();
            int markId = (int)READ_UINT();
            uint8_t paramCount = READ_BYTE();
            uint8_t handlerCount = READ_BYTE();

            // plus one for the implicit 'after' closure that will be called by COMPLETE
            ASSERT(VALUE_COUNT() >= handlerCount + paramCount + 1, "HANDLE did not have the required number of values on the stack.");

            ObjMarkFrame* frame = mochiNewMarkFrame(vm, markId, paramCount, handlerCount, ip + afterOffset);
            // take the handlers off the stack
            for (int i = 0; i < handlerCount; i++) {
                frame->handlers[i] = AS_CLOSURE(POP_VAL());
            }
            frame->afterClosure = AS_CLOSURE(POP_VAL());
            // take any handle parameters off the stack
            for (int i = 0; i < paramCount; i++) {
                frame->call.vars.slots[i] = POP_VAL();
            }

            PUSH_FRAME(frame);
            DISPATCH();
        }
        CASE_CODE(INJECT): {
            int markId = READ_UINT();

            for (int i = 0; i < fiber->frameStackTop - fiber->frameStack; i++) {
                ObjVarFrame* frame = *(fiber->frameStackTop - i - 1);
                if (frame->obj.type != OBJ_MARK_FRAME) {
                    continue;
                }
                ObjMarkFrame* mark = (ObjMarkFrame*)frame;
                if (mark->markId == markId) {
                    mark->nesting += 1;
                    if (mark->nesting == 1) {
                        break;
                    }
                }
            }

            DISPATCH();
        }
        CASE_CODE(EJECT): {
            int markId = READ_UINT();
            
            for (int i = 0; i < fiber->frameStackTop - fiber->frameStack; i++) {
                ObjVarFrame* frame = *(fiber->frameStackTop - i - 1);
                if (frame->obj.type != OBJ_MARK_FRAME) {
                    continue;
                }
                ObjMarkFrame* mark = (ObjMarkFrame*)frame;
                if (mark->markId == markId) {
                    mark->nesting -= 1;
                    if (mark->nesting <= 0) {
                        ASSERT(mark->nesting == 0, "EJECT instruction occurred without prior INJECT.");
                        break;
                    }
                }
            }

            DISPATCH();
        }
        CASE_CODE(COMPLETE): {
            ASSERT(FRAME_COUNT() > 0, "COMPLETE expects at least one mark frame on the frame stack.");

            ObjMarkFrame* frame = (ObjMarkFrame*)PEEK_FRAME(1);

            ObjCallFrame* newFrame = callClosureFrame(vm, fiber, frame->afterClosure, (ObjVarFrame*)frame, NULL, frame->call.afterLocation);

            DROP_FRAMES(1);
            PUSH_FRAME(newFrame);
            ip = frame->afterClosure->funcLocation;
            DISPATCH();
        }
        CASE_CODE(ESCAPE): {
            ASSERT(FRAME_COUNT() > 0, "ESCAPE expects at least one mark frame on the frame stack.");

            int markId = READ_UINT();
            uint8_t handlerIdx = READ_BYTE();
            int frameIdx = findFreeHandler(fiber, markId);
            ObjMarkFrame* frame = (ObjMarkFrame*)PEEK_FRAME(frameIdx + 1);
            ASSERT_OBJ_TYPE(frame, OBJ_MARK_FRAME, "ESCAPE expected to find a handle frame but found a different kind of frame..");

            ASSERT(handlerIdx < frame->handlerCount, "ESCAPE: Requested handler index outside the bounds of the mark frame handler set.");
            ObjClosure* handler = frame->handlers[handlerIdx];

            ObjCallFrame* newFrame = callClosureFrame(vm, fiber, handler, (ObjVarFrame*)frame, NULL, frame->call.afterLocation);

            ip = handler->funcLocation;
            // drop all frames up to and including the found mark frame
            DROP_FRAMES(frameIdx + 1);
            PUSH_FRAME(newFrame);

            DISPATCH();
        }
        CASE_CODE(REACT): {
            ASSERT(FRAME_COUNT() > 0, "REACT expects at least one handle frame on the frame stack.");

            int markId = READ_UINT();
            uint8_t handlerIdx = READ_BYTE();
            int frameIdx = findFreeHandler(fiber, markId);
            int frameCount = frameIdx + 1;
            ObjMarkFrame* frame = (ObjMarkFrame*)PEEK_FRAME(frameIdx + 1);

            ASSERT(handlerIdx < frame->handlerCount, "REACT: Requested handler index outside the bounds of the handle frame handler set.");
            ObjClosure* handler = frame->handlers[handlerIdx];

            // the major difference between REACT and ESCAPE is that REACT saves the current continuation
            ObjContinuation* cont = mochiNewContinuation(vm, ip, frame->call.vars.slotCount, VALUE_COUNT() - handler->paramCount, frameCount);
            // save all frames up to and including the found mark frame
            OBJ_ARRAY_COPY(cont->savedFrames, fiber->frameStackTop - frameCount, frameCount);
            valueArrayCopy(cont->savedStack, fiber->valueStack, cont->savedStackCount);

            mochiPushRoot(vm, (Obj*)cont);
            ObjCallFrame* newFrame = callClosureFrame(vm, fiber, handler, (ObjVarFrame*)frame, cont, frame->call.afterLocation);
            mochiPopRoot(vm);

            ip = handler->funcLocation;
            fiber->valueStackTop = fiber->valueStack;
            // drop all frames up to and including the found mark frame
            DROP_FRAMES(frameCount);
            PUSH_FRAME(newFrame);

            DISPATCH();
        }
        CASE_CODE(CALL_CONTINUATION): {
            ASSERT(VALUE_COUNT() > 0, "CALL_CONTINUATION expects at least one continuation value at the top of the value stack.");
            ObjContinuation* cont = AS_CONTINUATION(POP_VAL());
            mochiPushRoot(vm, (Obj*)cont);

            // the last frame in the saved frame stack is always the mark frame action reacted on
            ObjMarkFrame* mark = (ObjMarkFrame*)cont->savedFrames[0];
            ASSERT_OBJ_TYPE(mark, OBJ_MARK_FRAME, "CALL_CONTINUATION expected a mark frame at the bottom of the continuation frame stack.");
            ASSERT(VALUE_COUNT() > mark->call.vars.slotCount, "CALL_CONTINUATION expected more values on the value stack than were available for parameters.");

            // we basically copy it, but update the arguments passed along through the handling context and forget the 'return location'
            ObjMarkFrame* updated = mochiNewMarkFrame(vm, mark->markId, mark->call.vars.slotCount, mark->handlerCount, ip);
            updated->afterClosure = mark->afterClosure;
            OBJ_ARRAY_COPY(mark->handlers, updated->handlers, mark->handlerCount);
            // take any handle parameters off the stack
            for (int i = 0; i < mark->call.vars.slotCount; i++) {
                updated->call.vars.slots[i] = POP_VAL();
            }

            // captured stack values go under any remaining stack values
            int remainingValues = VALUE_COUNT();
            valueArrayCopy(fiber->valueStack + cont->savedStackCount, fiber->valueStack, remainingValues);
            valueArrayCopy(fiber->valueStack, cont->savedStack, cont->savedStackCount);
            fiber->valueStackTop = fiber->valueStackTop + cont->savedStackCount;

            // saved frames just go on top of the existing frames
            PUSH_FRAME(updated);
            OBJ_ARRAY_COPY(cont->savedFrames + 1, fiber->frameStackTop, cont->savedFramesCount - 1);
            fiber->frameStackTop = fiber->frameStackTop + (cont->savedFramesCount - 1);
            ip = cont->resumeLocation;

            mochiPopRoot(vm);
            DISPATCH();
        }
        CASE_CODE(TAILCALL_CONTINUATION): {
            ASSERT(VALUE_COUNT() > 0, "TAILCALL_CONTINUATION expects at least one continuation value at the top of the value stack.");
            ASSERT(FRAME_COUNT() > 0, "TAILCALL_CONTINUATION expects at least one call frame at the top of the frame stack.");
            ObjContinuation* cont = AS_CONTINUATION(POP_VAL());
            mochiPushRoot(vm, (Obj*)cont);

            uint8_t* after = ((ObjCallFrame*)POP_FRAME())->afterLocation;

            // the last frame in the saved frame stack is always the mark frame action reacted on
            ObjMarkFrame* mark = (ObjMarkFrame*)cont->savedFrames[0];
            ASSERT_OBJ_TYPE(mark, OBJ_MARK_FRAME, "TAILCALL_CONTINUATION expected a mark frame at the bottom of the continuation frame stack.");
            ASSERT(VALUE_COUNT() > mark->call.vars.slotCount, "TAILCALL_CONTINUATION expected more values on the value stack than were available for parameters.");

            // we basically copy it, but update the arguments passed along through the handling context and forget the 'return location'
            ObjMarkFrame* updated = mochiNewMarkFrame(vm, mark->markId, mark->call.vars.slotCount, mark->handlerCount, after);
            updated->afterClosure = mark->afterClosure;
            OBJ_ARRAY_COPY(mark->handlers, updated->handlers, mark->handlerCount);
            // take any handle parameters off the stack
            for (int i = 0; i < mark->call.vars.slotCount; i++) {
                updated->call.vars.slots[i] = POP_VAL();
            }

            // captured stack values go under any remaining stack values
            int remainingValues = VALUE_COUNT();
            valueArrayCopy(fiber->valueStack + cont->savedStackCount, fiber->valueStack, remainingValues);
            valueArrayCopy(fiber->valueStack, cont->savedStack, cont->savedStackCount);
            fiber->valueStackTop = fiber->valueStackTop + cont->savedStackCount;

            // saved frames just go on top of the existing frames
            PUSH_FRAME(updated);
            OBJ_ARRAY_COPY(cont->savedFrames + 1, fiber->frameStackTop, cont->savedFramesCount - 1);
            fiber->frameStackTop = fiber->frameStackTop + (cont->savedFramesCount - 1);
            ip = cont->resumeLocation;

            mochiPopRoot(vm);
            DISPATCH();
        }

        CASE_CODE(ZAP): {
            ASSERT(VALUE_COUNT() >= 1, "ZAP expects at least one value on the value stack.");
            DROP_VALS(1);
            DISPATCH();
        }
        CASE_CODE(SWAP): {
            ASSERT(VALUE_COUNT() >= 2, "SWAP expects at least two values on the value stack.");
            Value top = POP_VAL();
            Value below = POP_VAL();
            PUSH_VAL(top);
            PUSH_VAL(below);
            DISPATCH();
        }
        CASE_CODE(LIST_NIL): {
            PUSH_VAL(OBJ_VAL(NULL));
            DISPATCH();
        }
        CASE_CODE(LIST_CONS): {
            ASSERT(VALUE_COUNT() >= 2, "LIST_CONS expects at least two values on the value stack.");
            Value elem = PEEK_VAL(1);
            ObjList* tail = AS_LIST(PEEK_VAL(2));
            ObjList* new = mochiListCons(vm, elem, tail);
            DROP_VALS(2);
            PUSH_VAL(OBJ_VAL(new));
            DISPATCH();
        }
        CASE_CODE(LIST_HEAD): {
            ASSERT(VALUE_COUNT() >= 1, "LIST_HEAD expects at least one value on the value stack.");
            ObjList* list = AS_LIST(POP_VAL());
            // TODO: if empty list, throw an exception here? assumes built-in or standard library exception mechanism
            ASSERT(list != NULL, "LIST_HEAD cannot operate on an empty list.");
            ASSERT_OBJ_TYPE(list, OBJ_LIST, "LIST_HEAD can only operate on objects of list type.");
            PUSH_VAL(list->elem);
            DISPATCH();
        }
        CASE_CODE(LIST_TAIL): {
            ASSERT(VALUE_COUNT() >= 1, "LIST_HEAD expects at least one value on the value stack.");
            ObjList* list = AS_LIST(POP_VAL());
            // TODO: if empty list, throw an exception here? assumes built-in or standard library exception mechanism
            ASSERT(list != NULL, "LIST_HEAD cannot operate on an empty list.");
            ASSERT_OBJ_TYPE(list, OBJ_LIST, "LIST_HEAD can only operate on objects of list type.");
            PUSH_VAL(OBJ_VAL(list->next));
            DISPATCH();
        }
        CASE_CODE(LIST_IS_EMPTY): {
            ASSERT(VALUE_COUNT() >= 1, "LIST_IS_EMPTY expects at least one value on the stack.");
            ObjList* list = AS_LIST(POP_VAL());
            PUSH_VAL(BOOL_VAL(list == NULL));
            DISPATCH();
        }
    }

    UNREACHABLE();
    return MOCHIVM_RESULT_RUNTIME_ERROR;

#undef READ_BYTE
#undef READ_SHORT
#undef READ_USHORT
#undef READ_UINT
#undef READ_CONSTANT
}

MochiVMInterpretResult mochiInterpret(MochiVM* vm, ObjFiber* fiber) {
    fiber->ip = vm->block->code.data;
    return run(vm, fiber);
}