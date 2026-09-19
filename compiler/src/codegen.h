# pragma once

#include "irgenerator.h" // for Function, Quad, Operand, Location
#include <string>
#include <sstream>
#include <vector>
#include <memory>

// Lowers Function/BasicBlock/Quad IR (already run through RegisterAllocator)
// into real RV32IM assembly text, one instruction per IR op.
//
// Every function follows the same convention, including the implicit
// top-level "<entry>" function -- there is no special-casing for it here.
// A fixed, separately-emitted _start stub calls it exactly like any other
// function (jal ra, ...), then halts. This keeps every function's
// prologue/epilogue uniform: entry only differs from a user function by
// who calls it, not by how it's generated.
//
// Scope note: straight-line arithmetic, return, branches, and function
// calls (PARAM/CALL, up to 8 args -- no stack-passed args yet) are all
// lowered. Anything else still hits a "not yet supported" throw.
class Codegen {
public:
	std::string generate(const std::vector<std::unique_ptr<Function>>& functions);

private:
	std::ostringstream out_;

	// how many PARAM quads have been seen since the last CALL -- decides
	// which of a0..a7 the next PARAM's value lands in. Reset to 0 by
	// every CALL (and defensively at the top of each function).
	int pendingParamCount_ = 0;

	void emitFunction(const Function& fn);
	void emitQuad(const Function& fn, const Quad& q);

	// Loads an operand's value into `reg`. Handles VREG (load from its
	// stack slot) and IMM (materialize via addi) -- the only operand kinds
	// straight-line arithmetic needs.
	void loadOperand(const Function& fn, const Operand& op, const std::string& reg);

	// Stores `reg`'s value into a VREG operand's stack slot.
	void storeResult(const Function& fn, const Operand& dest, const std::string& reg);

	std::string funcLabel(const Function& fn);

	// Block ids reset to 1 per function (see IRGenerator::createBlock), so
	// a label needs to be qualified by function to avoid collisions -- L2
	// in one function is unrelated to L2 in another.
	std::string blockLabel(const Function& fn, int blockId);
};