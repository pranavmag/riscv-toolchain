#include "codegen.h"
#include <stdexcept>

std::string Codegen::funcLabel(const Function& fn) {
	return "func_" + std::to_string(fn.id);
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

	// prologue -- fp-relative addressing isn't valid until the last line here,
	// so ra/old-fp are saved sp-relative first
	out_ << "    addi sp, sp, -" << fn.frameSize << "\n";
	out_ << "    sw   ra, " << (fn.frameSize - 4) << "(sp)\n";
	out_ << "    sw   fp, " << (fn.frameSize - 8) << "(sp)\n";
	out_ << "    addi fp, sp, " << fn.frameSize << "\n";

	for (const auto& block : fn.cfg) {
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
	out_ << "    jal  ra, " << "func_0" << "\n"; // <entry> is always Function id 0
	out_ << "    addi a7, x0, 93\n";
	out_ << "    ecall\n\n";

	for (const auto& fn : functions) {
		emitFunction(*fn);
		out_ << "\n";
	}

	return out_.str();
}