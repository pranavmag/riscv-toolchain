#include "regalloc.h"

void RegisterAllocator::allocate(Function& fn) {
	fn.vregLocations.clear();

	for (int vreg = 1; vreg < fn.vregCounter; ++vreg) {
		int offset = -(vreg * WORD_SIZE);
		fn.vregLocations[vreg] = Location{ LocationKind::StackSlot, offset };
	}

	int rawSize = (fn.vregCounter - 1) * WORD_SIZE;
	fn.frameSize = ((rawSize + STACK_ALIGN - 1) / STACK_ALIGN) * STACK_ALIGN;
}