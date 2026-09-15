#include "regalloc.h"

void RegisterAllocator::allocate(Function& fn) {
	fn.vregLocations.clear();

	// The top of every frame (the words closest to fp) is reserved for the
	// saved return address and the caller's frame pointer -- Codegen's
	// prologue/epilogue depend on finding them at fixed offsets -4(fp) and
	// -8(fp) regardless of how many vregs a function uses. vreg slots start
	// below that reserved space, not at -4(fp).
	constexpr int SAVED_REGS_SIZE = 2 * WORD_SIZE; // ra + caller's fp

	for (int vreg = 1; vreg < fn.vregCounter; ++vreg) {
		int offset = -(SAVED_REGS_SIZE + vreg * WORD_SIZE);
		fn.vregLocations[vreg] = Location{ LocationKind::StackSlot, offset };
	}

	int rawSize = SAVED_REGS_SIZE + (fn.vregCounter - 1) * WORD_SIZE;
	fn.frameSize = ((rawSize + STACK_ALIGN - 1) / STACK_ALIGN) * STACK_ALIGN;
}