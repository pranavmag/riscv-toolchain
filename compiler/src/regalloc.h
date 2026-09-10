# pragma once

#include "irgenerator.h"

// Naive, stack-based register allocator: every vreg gets its own
// permanent 4-byte stack slot for the function's entire lifetime.
// No liveness analysis, no slot reuse, no real physical registers
// ever handed out

class RegisterAllocator {
public:
	void allocate(Function& fn);

private:
	static constexpr int WORD_SIZE = 4;
	static constexpr int STACK_ALIGN = 16;
};