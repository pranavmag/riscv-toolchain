# pragma once

#include "irgenerator.h"
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
// Scope note: this first pass only lowers what straight-line arithmetic
// and `return` need (LOAD_IMM, MOV, ADD/SUB/MUL/DIV/REM, RET). Anything
// else (branches, calls) hits the "not yet supported" throw further down --
// deliberately, so an unsupported program fails loudly instead of silently
// emitting wrong code.
class Codegen {
public:
	std::string generate(const std::vector<std::unique_ptr<Function>>& functions);

private:
	std::ostringstream out_;

	void emitFunction(const Function& fn);
	void emitQuad(const Function& fn, const Quad& q);

	// Loads an operand's value into `reg`. Handles VREG (load from its
	// stack slot) and IMM (materialize via addi) -- the only operand kinds
	// straight-line arithmetic needs.
	void loadOperand(const Function& fn, const Operand& op, const std::string& reg);

	// Stores `reg`'s value into a VREG operand's stack slot.
	void storeResult(const Function& fn, const Operand& dest, const std::string& reg);

	std::string funcLabel(const Function& fn);
};