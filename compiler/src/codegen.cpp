#include "codegen.h"
#include <stdexcept>

std::string Codegen::funcLabel(const Function& fn) {
	return "func_" + std::to_string(fn.id);
}

std::string Codegen::blockLabel(const Function& fn, int blockId) {
	return funcLabel(fn) + "_L" + std::to_string(blockId);
}

void Codegen::loadOperand(const Function& fn, const Operand& op, const std::string& reg) {
	if (op.type == OperandType::VREG) {
		auto it = fn.vregLocations.find(op.value);
		if (it == fn.vregLocations.end() || it->second.kind != LocationKind::StackSlot) {
			throw std::runtime_error("Codegen: vreg has no allocated stack slot");
		}
		out_ << "    lw   " << reg << ", " << it->second.value << "(fp)\n";
	}
	else if (op.type == OperandType::IMM) {
		out_ << "    addi " << reg << ", x0, " << op.value << "\n";
	}
	else {
		throw std::runtime_error("Codegen: unsupported operand kind in loadOperand");
	}
}

void Codegen::storeResult(const Function& fn, const Operand& dest, const std::string& reg) {
	if (dest.type != OperandType::VREG) {
		throw std::runtime_error("Codegen: storeResult target is not a vreg");
	}
	auto it = fn.vregLocations.find(dest.value);
	if (it == fn.vregLocations.end() || it->second.kind != LocationKind::StackSlot) {
		throw std::runtime_error("Codegen: dest vreg has no allocated stack slot");
	}
	out_ << "    sw   " << reg << ", " << it->second.value << "(fp)\n";
}

void Codegen::emitQuad(const Function& fn, const Quad& q) {
	switch (q.op) {
	case IROp::LOAD_IMM: {
		loadOperand(fn, q.src1, "t0"); // src1 is the IMM
		storeResult(fn, q.dest, "t0");
		break;
	}
	case IROp::MOV: {
		loadOperand(fn, q.src1, "t0");
		storeResult(fn, q.dest, "t0");
		break;
	}
	case IROp::ADD:
	case IROp::SUB:
	case IROp::MUL:
	case IROp::DIV:
	case IROp::REM: {
		loadOperand(fn, q.src1, "t0");
		loadOperand(fn, q.src2, "t1");
		std::string mnemonic;
		switch (q.op) {
		case IROp::ADD: mnemonic = "add"; break;
		case IROp::SUB: mnemonic = "sub"; break;
		case IROp::MUL: mnemonic = "mul"; break;
		case IROp::DIV: mnemonic = "div"; break;
		case IROp::REM: mnemonic = "rem"; break;
		default: break; // unreachable, silences -Wswitch
		}
		out_ << "    " << mnemonic << "  t2, t0, t1\n";
		storeResult(fn, q.dest, "t2");
		break;
	}
	case IROp::EQ:
	case IROp::NE:
	case IROp::LT:
	case IROp::LE:
	case IROp::GT:
	case IROp::GE: {
		// base RV32I has no seq/sne/sle/sge -- built from slt/sltiu/xor
		loadOperand(fn, q.src1, "t0");
		loadOperand(fn, q.src2, "t1");
		switch (q.op) {
		case IROp::LT:
			out_ << "    slt  t2, t0, t1\n"; // t2 = (src1 < src2)
			break;
		case IROp::GT:
			out_ << "    slt  t2, t1, t0\n"; // t2 = (src2 < src1) == (src1 > src2)
			break;
		case IROp::LE:
			out_ << "    slt   t2, t1, t0\n";  // t2 = (src2 < src1)
			out_ << "    sltiu t2, t2, 1\n";   // t2 = !(src2 < src1) == (src1 <= src2)
			break;
		case IROp::GE:
			out_ << "    slt   t2, t0, t1\n";  // t2 = (src1 < src2)
			out_ << "    sltiu t2, t2, 1\n";   // t2 = !(src1 < src2) == (src1 >= src2)
			break;
		case IROp::EQ:
			out_ << "    xor   t2, t0, t1\n";  // t2 = 0 iff equal
			out_ << "    sltiu t2, t2, 1\n";   // t2 = (t2 < 1) == (src1 == src2)
			break;
		case IROp::NE:
			out_ << "    xor  t2, t0, t1\n";   // t2 = 0 iff equal
			out_ << "    sltu t2, x0, t2\n";   // t2 = (0 < t2) == (src1 != src2)
			break;
		default: break; // unreachable, silences -Wswitch
		}
		storeResult(fn, q.dest, "t2");
		break;
	}
	case IROp::BEQ: {
		// visitIf/visitWhile always build this as: cond in src1, the
		// comparison-against-zero encoded structurally (src2 is always
		// the literal 0), target block id in dest as a LABEL operand
		loadOperand(fn, q.src1, "t0");
		out_ << "    beq  t0, x0, " << blockLabel(fn, q.dest.value) << "\n";
		break;
	}
	case IROp::JUMP: {
		out_ << "    jal  x0, " << blockLabel(fn, q.dest.value) << "\n";
		break;
	}
	case IROp::PARAM: {
		static const char* argRegs[] = { "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7" };
		if (pendingParamCount_ >= 8) {
			throw std::runtime_error("Codegen: more than 8 arguments not supported (no stack-passed args yet)");
		}
		loadOperand(fn, q.src1, argRegs[pendingParamCount_]);
		pendingParamCount_++;
		break;
	}
	case IROp::CALL: {
		// src1 is {FUNC, calleeId} -- see visitFuncCallNode. Function ids
		// are assigned in creation order starting at 0 and match funcLabel's
		// "func_<id>" scheme directly, so no Function lookup is needed here.
		out_ << "    jal  ra, func_" << q.src1.value << "\n";
		pendingParamCount_ = 0;
		storeResult(fn, q.dest, "a0");
		break;
	}
	case IROp::PRINT: {
		loadOperand(fn, q.src1, "a0");
		out_ << "    addi a7, x0, 1\n";
		out_ << "    ecall\n";
		break;
	}
	case IROp::RET: {
		// visitRet puts the return value's vreg in dest, not src1
		if (q.dest.type == OperandType::VREG) {
			loadOperand(fn, q.dest, "a0");
		}
		// epilogue -- mirrors emitFunction's prologue exactly
		out_ << "    lw   ra, -4(fp)\n";
		out_ << "    lw   t0, -8(fp)\n";
		out_ << "    addi sp, fp, 0\n"; // sp = fp (fp already equals the pre-prologue sp)
		out_ << "    addi fp, t0, 0\n"; // fp = saved caller's fp
		out_ << "    jalr x0, ra, 0\n";
		break;
	}
	default:
		throw std::runtime_error("Codegen: IROp not yet supported (scope: straight-line arithmetic + return only)");
	}
}

void Codegen::emitFunction(const Function& fn) {
	out_ << funcLabel(fn) << ":\n";

	pendingParamCount_ = 0;

	// prologue -- fp-relative addressing isn't valid until the last line here,
	// so ra/old-fp are saved sp-relative first
	out_ << "    addi sp, sp, -" << fn.frameSize << "\n";
	out_ << "    sw   ra, " << (fn.frameSize - 4) << "(sp)\n";
	out_ << "    sw   fp, " << (fn.frameSize - 8) << "(sp)\n";
	out_ << "    addi fp, sp, " << fn.frameSize << "\n";

	// copy incoming arguments (a0..a7, per ABI convention -- see the PARAM
	// case above) into each parameter's own stack slot, so the body reads
	// them exactly like any other vreg
	static const char* argRegs[] = { "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7" };
	if (fn.paramVRegs.size() > 8) {
		throw std::runtime_error("Codegen: more than 8 parameters not supported (no stack-passed args yet)");
	}
	for (size_t i = 0; i < fn.paramVRegs.size(); ++i) {
		Operand paramDest{ OperandType::VREG, fn.paramVRegs[i] };
		storeResult(fn, paramDest, argRegs[i]);
	}

	for (const auto& block : fn.cfg) {
		out_ << blockLabel(fn, block->id) << ":\n";
		for (const auto& quad : block->instructions) {
			emitQuad(fn, quad);
		}
	}
}

std::string Codegen::generate(const std::vector<std::unique_ptr<Function>>& functions) {
	out_.str("");

	out_ << ".text\n";
	out_ << ".globl _start\n\n";

	// entry is just another function from Codegen's point of view --
	// _start is the only thing that treats it specially, by calling it
	out_ << "_start:\n";
	out_ << "    jal  ra, func_0\n"; // <entry> always runs first, like C's global-init prelude

	// if the user declared their own main(), it's the program's real
	// entry point beyond that prelude -- call it too, and let its
	// return value become the actual exit code
	const Function* userMain = nullptr;
	for (const auto& fn : functions) {
		if (fn->name == "main") {
			userMain = fn.get();
			break;
		}
	}
	if (userMain != nullptr) {
		out_ << "    jal  ra, " << funcLabel(*userMain) << "\n";
	}

	out_ << "    addi a7, x0, 93\n";
	out_ << "    ecall\n\n";

	for (const auto& fn : functions) {
		emitFunction(*fn);
		out_ << "\n";
	}

	return out_.str();
}