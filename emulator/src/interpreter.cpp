#include "decoder.h"
#include "interpreter.h"
#include <vector>
#include <array>
#include <iostream>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <fstream>

void Memory::loadProgram(const std::string& fileName) {

	std::ifstream istrm(fileName, std::ios::binary);
	if (!istrm.is_open()) {
		std::cout << "failed to open " << fileName << '\n';
	}
	else {
		istrm.seekg(0, std::ios::end);
		size_t fileSize = istrm.tellg();
		istrm.seekg(0, std::ios::beg);
		istrm.read(reinterpret_cast<char*>(memory.data()), fileSize);
	}
}

void Core::writeReg(int regNum, uint32_t value) {
	if (regNum != 0) {
		registers[regNum] = value;
	}
}

void Core::writeFReg(int regNum, float value) {
	freg[regNum] = value;
}

uint32_t Core::readReg(int regNum) {
	return registers[regNum];
}

float Core::readFReg(int regNum) {
	return freg[regNum];
}


uint32_t Memory::readByte(uint32_t addr) {
	return memory[addr];
}

uint32_t Memory::readHalfWord(uint32_t addr) {
	return memory[addr] |
		(memory[addr + 1] << 8);
}

uint32_t Memory::readWord(uint32_t addr) {
	return  memory[addr] |
		(memory[addr + 1] << 8) |
		(memory[addr + 2] << 16) |
		(memory[addr + 3] << 24);
}

void Memory::writeByte(uint32_t addr, uint32_t value) {
	memory[addr] = value & 0xFF;
}

void Memory::writeHalfWord(uint32_t addr, uint32_t value) {
	memory[addr] = value & 0xFF;
	memory[addr + 1] = (value >> 8) & 0xFF;
}

void Memory::writeWord(uint32_t addr, uint32_t value) {
	memory[addr] = value & 0xFF;
	memory[addr + 1] = (value >> 8) & 0xFF;
	memory[addr + 2] = (value >> 16) & 0xFF;
	memory[addr + 3] = (value >> 24) & 0xFF;
}

bool writesToRegister(InstructionType type) {
	if (type == InstructionType::S || type == InstructionType::FPS || type == InstructionType::B ||
		type == InstructionType::ENVIRONMENT || type == InstructionType::UNKNOWN) {
		return false;
	}
	return true;
}

bool usesRs1(InstructionType type) {
	if (type != InstructionType::U && type != InstructionType::J && type != InstructionType::ENVIRONMENT) {
		return true;
	}
	return false;
}

bool usesRs2(InstructionType type) {
	if (type == InstructionType::R || type == InstructionType::B || type == InstructionType::S || type == InstructionType::FPA ||
		type == InstructionType::FPR4 || type == InstructionType::FMINMAX || type == InstructionType::FCOMP || type == InstructionType::FPS) {
		return true;
	}
	return false;
}

bool usesRs3(InstructionType type) {
	if (type == InstructionType::FPR4) {
		return true;
	}
	return false;
}

bool isFloat(InstructionType type) {
	if (type == InstructionType::FPL || type == InstructionType::FPA || type == InstructionType::FPR1 ||
		type == InstructionType::FMINMAX || type == InstructionType::FPR4 || type == InstructionType::INTCONVFP) {
		return true;
	}
	return false;
}

void Core::fetchStage(Memory& mem) {
	if (loadUseHazard) {
		next_if_id_reg = if_id_reg;
		return;
	}

	uint32_t rawInstruction = mem.readWord(pc);

	next_if_id_reg.bits = rawInstruction;
	next_if_id_reg.pc = pc;
	next_if_id_reg.bubble = false;


	pc += 4;
}

void Core::decodeStage(Memory& mem) {
	(void)mem;

	if (if_id_reg.bubble) {
		next_id_ex_reg.bubble = true;
		return;
	}

	DecodedInstruction inst = decodeInstruction(if_id_reg.bits);

	bool isLoadUse = (id_ex_reg.inst.type == InstructionType::LOAD || id_ex_reg.inst.type == InstructionType::FPL);

	if (!id_ex_reg.bubble && isLoadUse && id_ex_reg.inst.rd != 0) {
		bool uses1 = usesRs1(inst.type);
		bool uses2 = usesRs2(inst.type);
		bool uses3 = usesRs3(inst.type);

		if ((uses1 && id_ex_reg.inst.rd == inst.rs1) || (uses2 && id_ex_reg.inst.rd == inst.rs2) ||
			(uses3 && id_ex_reg.inst.rd == inst.rs3)) {
			loadUseHazard = true;
			next_id_ex_reg.bubble = true;
			return;
		}
	}

	loadUseHazard = false;

	uint32_t rs1_value = readReg(inst.rs1);
	uint32_t rs2_value = readReg(inst.rs2);

	float rs1_fvalue = readFReg(inst.rs1);
	float rs2_fvalue = readFReg(inst.rs2);
	float rs3_fvalue = readFReg(inst.rs3);

	next_id_ex_reg.inst = inst;
	next_id_ex_reg.pc = if_id_reg.pc;
	next_id_ex_reg.rs1Value = rs1_value;
	next_id_ex_reg.rs2Value = rs2_value;
	next_id_ex_reg.rs1FValue = rs1_fvalue;
	next_id_ex_reg.rs2FValue = rs2_fvalue;
	next_id_ex_reg.rs3FValue = rs3_fvalue;
	next_id_ex_reg.bubble = false;
}

void Core::executeStage(Memory& mem) {
	(void)mem;
	
	if (id_ex_reg.bubble) {
		next_ex_mem_reg.bubble = true;
		return;
	}

	DecodedInstruction inst = id_ex_reg.inst;

	uint32_t aluResult{};
	float aluFResult{};

	// Forwarding Engine

	// MEM Hazard
	if (!mem_wb_reg.bubble && writesToRegister(mem_wb_reg.inst.type)) {
		bool memIsFloat = isFloat(mem_wb_reg.inst.type);
		bool memUsesRs1 = usesRs1(inst.type);
		bool memUsesRs2 = usesRs2(inst.type);
		bool memUsesRs3 = usesRs3(inst.type);

		if (memIsFloat || mem_wb_reg.inst.rd != 0) {
			uint32_t memInt = (mem_wb_reg.inst.type == InstructionType::LOAD) ? mem_wb_reg.memRead : mem_wb_reg.aluResult;
			float memFloat = (mem_wb_reg.inst.type == InstructionType::FPL) ? mem_wb_reg.memFRead : mem_wb_reg.aluFResult;
			if (memUsesRs1 && inst.rs1 == mem_wb_reg.inst.rd) {
				if (memIsFloat) id_ex_reg.rs1FValue = memFloat; else id_ex_reg.rs1Value = memInt;
			}
			if (memUsesRs2 && inst.rs2 == mem_wb_reg.inst.rd) {
				if (memIsFloat) id_ex_reg.rs2FValue = memFloat; else id_ex_reg.rs2Value = memInt;
			}
			if (memUsesRs3 && inst.rs3 == mem_wb_reg.inst.rd) {
				if (memIsFloat) id_ex_reg.rs3FValue = memFloat;
			}
		}
	}

	// EX Hazard
	if (!ex_mem_reg.bubble && writesToRegister(ex_mem_reg.inst.type)) {
		bool exIsFloat = isFloat(ex_mem_reg.inst.type);
		bool exUsesRs1 = usesRs1(inst.type);
		bool exUsesRs2 = usesRs2(inst.type);
		bool exUsesRs3 = usesRs3(inst.type);

		if (exIsFloat || ex_mem_reg.inst.rd != 0) {
			if (exUsesRs1 && inst.rs1 == ex_mem_reg.inst.rd) {
				if (exIsFloat) id_ex_reg.rs1FValue = ex_mem_reg.aluFResult; else id_ex_reg.rs1Value = ex_mem_reg.aluResult;
			}
			if (exUsesRs2 && inst.rs2 == ex_mem_reg.inst.rd) {
				if (exIsFloat) id_ex_reg.rs2FValue = ex_mem_reg.aluFResult; else id_ex_reg.rs2Value = ex_mem_reg.aluResult;
			}
			if (exUsesRs3 && inst.rs3 == ex_mem_reg.inst.rd) {
				if (exIsFloat) id_ex_reg.rs3FValue = ex_mem_reg.aluFResult;
			}
		}
	}

	// Execute Switch
	switch (inst.name) {
	case Instruction::ADDI: {
		aluResult = id_ex_reg.rs1Value + inst.imm;
		break;
	}
	case Instruction::ANDI: {
		aluResult = id_ex_reg.rs1Value & inst.imm;
		break;
	}
	case Instruction::ORI: {
		aluResult = id_ex_reg.rs1Value | inst.imm;
		break;
	}
	case Instruction::XORI: {
		aluResult = id_ex_reg.rs1Value ^ inst.imm;
		break;
	}
	case Instruction::SLTI: {
		aluResult = static_cast<int32_t>(id_ex_reg.rs1Value) < inst.imm ? 1 : 0;
		break;
	}
	case Instruction::SLTIU: {
		aluResult = (id_ex_reg.rs1Value) < static_cast<uint32_t>(inst.imm) ? 1 : 0;
		break;
	}
	case Instruction::SLLI: {
		aluResult = id_ex_reg.rs1Value << inst.shamt;
		break;
	}
	case Instruction::SRLI: {
		aluResult = id_ex_reg.rs1Value >> inst.shamt;
		break;
	}
	case Instruction::SRAI: {
		aluResult = static_cast<uint32_t>(static_cast<int32_t>(id_ex_reg.rs1Value) >> inst.shamt);
		break;
	}
	case Instruction::ADD: {
		aluResult = id_ex_reg.rs1Value + id_ex_reg.rs2Value;
		break;
	}
	case Instruction::SUB: {
		aluResult = id_ex_reg.rs1Value - id_ex_reg.rs2Value;
		break;
	}
	case Instruction::AND: {
		aluResult = id_ex_reg.rs1Value & id_ex_reg.rs2Value;
		break;
	}
	case Instruction::OR: {
		aluResult = id_ex_reg.rs1Value | id_ex_reg.rs2Value;
		break;
	}
	case Instruction::XOR: {
		aluResult = id_ex_reg.rs1Value ^ id_ex_reg.rs2Value;
		break;
	}
	case Instruction::SLL: {
		aluResult = id_ex_reg.rs1Value << id_ex_reg.rs2Value;
		break;
	}
	case Instruction::SRL: {
		aluResult = id_ex_reg.rs1Value >> id_ex_reg.rs2Value;
		break;
	}
	case Instruction::SRA: {
		aluResult = static_cast<uint32_t>(static_cast<int32_t>(id_ex_reg.rs1Value) >> id_ex_reg.rs2Value);
		break;
	}
	case Instruction::SLT: {
		aluResult = static_cast<int32_t>(id_ex_reg.rs1Value) < static_cast<int32_t>(id_ex_reg.rs2Value) ? 1 : 0;
		break;
	}
	case Instruction::SLTU: {
		aluResult = id_ex_reg.rs1Value < id_ex_reg.rs2Value ? 1 : 0;
		break;
	}
	case Instruction::BEQ: {
		if (id_ex_reg.rs1Value == id_ex_reg.rs2Value) {
			next_ex_mem_reg.takeBranch = true;
			next_ex_mem_reg.branchTarget = id_ex_reg.pc + inst.imm;
		}
		break;
	}
	case Instruction::BNE: {
		if (id_ex_reg.rs1Value != id_ex_reg.rs2Value) {
			next_ex_mem_reg.takeBranch = true;
			next_ex_mem_reg.branchTarget = id_ex_reg.pc + inst.imm;
		}
		break;
	}
	case Instruction::BLT: {
		if (static_cast<int32_t>(id_ex_reg.rs1Value) < static_cast<int32_t>(id_ex_reg.rs2Value)) {
			next_ex_mem_reg.takeBranch = true;
			next_ex_mem_reg.branchTarget = id_ex_reg.pc + inst.imm;
		}
		break;
	}
	case Instruction::BLTU: {
		if (id_ex_reg.rs1Value < id_ex_reg.rs2Value) {
			next_ex_mem_reg.takeBranch = true;
			next_ex_mem_reg.branchTarget = id_ex_reg.pc + inst.imm;
		}
		break;
	}
	case Instruction::BGE: {
		if (static_cast<int32_t>(id_ex_reg.rs1Value) >= static_cast<int32_t>(id_ex_reg.rs2Value)) {
			next_ex_mem_reg.takeBranch = true;
			next_ex_mem_reg.branchTarget = id_ex_reg.pc + inst.imm;
		}
		break;
	}
	case Instruction::BGEU: {
		if (id_ex_reg.rs1Value >= id_ex_reg.rs2Value) {
			next_ex_mem_reg.takeBranch = true;
			next_ex_mem_reg.branchTarget = id_ex_reg.pc + inst.imm;
		}
		break;
	}
	case Instruction::JAL: {
		next_ex_mem_reg.takeBranch = true;
		next_ex_mem_reg.branchTarget = id_ex_reg.pc + inst.imm;
		aluResult = id_ex_reg.pc + 4;
		break;
	}
	case Instruction::JALR: {
		next_ex_mem_reg.takeBranch = true;
		next_ex_mem_reg.branchTarget = (id_ex_reg.rs1Value + inst.imm) & 0xFFFFFFFE;
		aluResult = id_ex_reg.pc + 4;
		break;
	}
	case Instruction::LB:
	case Instruction::LBU:
	case Instruction::LH:
	case Instruction::LHU:
	case Instruction::LW: 
		aluResult = id_ex_reg.rs1Value + inst.imm;
		break;
	case Instruction::SB:
	case Instruction::SH:
	case Instruction::SW:
		aluResult = id_ex_reg.rs1Value + inst.imm;
		break;

	case Instruction::AUIPC: {
		aluResult = id_ex_reg.pc + inst.imm;
		break;
	}
	case Instruction::LUI: {
		aluResult = inst.imm;
		break;
	}
	case Instruction::MUL: {
		aluResult = id_ex_reg.rs1Value * id_ex_reg.rs2Value;
		break;
	}
	case Instruction::MULH: {
		int64_t result = int64_t((int32_t)id_ex_reg.rs1Value) * int64_t((int32_t)id_ex_reg.rs2Value);
		aluResult = result >> 32;
		break;
	}
	case Instruction::MULHU: {
		uint64_t result = uint64_t((uint32_t)id_ex_reg.rs1Value) * uint64_t((uint32_t)id_ex_reg.rs2Value);
		aluResult = result >> 32;
		break;
	}
	case Instruction::MULHSU: {
		int64_t result = int64_t((int32_t)id_ex_reg.rs1Value) * int64_t((uint32_t)id_ex_reg.rs2Value);
		aluResult = result >> 32;
		break;
	}
	case Instruction::DIV: {
		int32_t dividend = (int32_t)id_ex_reg.rs1Value;
		int32_t divisor = (int32_t)id_ex_reg.rs2Value;
		if (divisor == 0) {
			aluResult = -1;
		}
		else if (dividend == INT32_MIN && divisor == -1) {
			aluResult = INT32_MIN;
		}
		else {
			aluResult = dividend / divisor;
		}
		break;
	}
	case Instruction::DIVU: {
		uint32_t dividend = id_ex_reg.rs1Value;
		uint32_t divisor = id_ex_reg.rs2Value;
		aluResult = divisor == 0 ? -1 : (dividend / divisor);
		break;
	}
	case Instruction::REM: {
		int32_t dividend = (int32_t)id_ex_reg.rs1Value;
		int32_t divisor = (int32_t)id_ex_reg.rs2Value;
		if (divisor == 0) {
			aluResult = dividend;
		}
		else if (dividend == INT32_MIN && divisor == -1) {
			aluResult = 0;
		}
		else {
			aluResult = dividend % divisor;
		}
		break;
	}
	case Instruction::REMU: {
		uint32_t dividend = id_ex_reg.rs1Value;
		uint32_t divisor = id_ex_reg.rs2Value;
		aluResult = divisor == 0 ? dividend : (dividend % divisor);
		break;
	}
	case Instruction::FLW: {
		aluResult = id_ex_reg.rs1Value + inst.imm;
		break;
	}
	case Instruction::FSW: {
		aluResult = id_ex_reg.rs1Value + inst.imm;
		break;
	}
	case::Instruction::FADDS: {
		aluFResult = id_ex_reg.rs1FValue + id_ex_reg.rs2FValue;
		break;
	}
	case::Instruction::FMULS: {
		aluFResult = id_ex_reg.rs1FValue * id_ex_reg.rs2FValue;
		break;
	}
	case::Instruction::FSUBS: {
		aluFResult = id_ex_reg.rs1FValue - id_ex_reg.rs2FValue;
		break;
	}
	case::Instruction::FDIVS: {
		aluFResult = id_ex_reg.rs1FValue / id_ex_reg.rs2FValue;
		break;
	}
	case::Instruction::FSQRT: {
		float value = id_ex_reg.rs1FValue;
		aluFResult = std::sqrt(value);
		break;
	}
	case::Instruction::FMIN: {
		float value1 = id_ex_reg.rs1FValue;
		float value2 = id_ex_reg.rs2FValue;
		aluFResult = std::fmin(value1, value2);
		break;
	}
	case::Instruction::FMAX: {
		float value1 = id_ex_reg.rs1FValue;
		float value2 = id_ex_reg.rs2FValue;
		aluFResult = std::fmax(value1, value2);
		break;
	}
	case::Instruction::FMADDS: {
		float rs1 = id_ex_reg.rs1FValue;
		float rs2 = id_ex_reg.rs2FValue;
		float rs3 = id_ex_reg.rs3FValue;

		aluFResult = std::fmaf(rs1, rs2, rs3);
		break;
	}
	case::Instruction::FMSUBS: {
		float rs1 = id_ex_reg.rs1FValue;
		float rs2 = id_ex_reg.rs2FValue;
		float rs3 = id_ex_reg.rs3FValue;

		aluFResult = std::fmaf(rs1, rs2, -rs3);
		break;
	}
	case::Instruction::FNMSUBS: {
		float rs1 = id_ex_reg.rs1FValue;
		float rs2 = id_ex_reg.rs2FValue;
		float rs3 = id_ex_reg.rs3FValue;

		aluFResult = std::fmaf(-rs1, rs2, rs3);
		break;
	}
	case::Instruction::FNMADDS: {
		float rs1 = id_ex_reg.rs1FValue;
		float rs2 = id_ex_reg.rs2FValue;
		float rs3 = id_ex_reg.rs3FValue;

		aluFResult = std::fmaf(-rs1, rs2, -rs3);
		break;
	}
	case::Instruction::FCVTWS: {
		float f = id_ex_reg.rs1FValue;
		aluResult = static_cast<int32_t>(f);
		break;
	}
	case::Instruction::FCVTSW: {
		int32_t value = id_ex_reg.rs1Value;
		aluFResult = static_cast<float>(value);
		break;
	}
	case::Instruction::FCVTWUS: {
		float f = id_ex_reg.rs1FValue;
		aluResult = static_cast<uint32_t>(f);
		break;
	}
	case::Instruction::FCVTSWU: {
		uint32_t value = id_ex_reg.rs1Value;
		aluFResult = static_cast<float>(value);
		break;
	}
	case::Instruction::FSGNJS: {
		float rs1 = id_ex_reg.rs1FValue;
		float rs2 = id_ex_reg.rs2FValue;
		uint32_t bits1{};
		uint32_t bits2{};

		std::memcpy(&bits1, &rs1, sizeof(bits1));
		std::memcpy(&bits2, &rs2, sizeof(bits2));

		uint32_t result = (bits2 & 0x80000000) | (bits1 & 0x7FFFFFFF);

		std::memcpy(&aluFResult, &result, sizeof(aluFResult));
		break;
	}
	case::Instruction::FSGNJNS: {
		float rs1 = id_ex_reg.rs1FValue;
		float rs2 = id_ex_reg.rs2FValue;
		uint32_t bits1{};
		uint32_t bits2{};

		std::memcpy(&bits1, &rs1, sizeof(bits1));
		std::memcpy(&bits2, &rs2, sizeof(bits2));

		uint32_t result = (bits2 ^ 0x80000000) | (bits1 & 0x7FFFFFFF);

		std::memcpy(&aluFResult, &result, sizeof(aluFResult));

		break;
	}
	case::Instruction::FSGNJXS: {
		float rs1 = id_ex_reg.rs1FValue;
		float rs2 = id_ex_reg.rs2FValue;
		uint32_t bits1{};
		uint32_t bits2{};

		std::memcpy(&bits1, &rs1, sizeof(bits1));
		std::memcpy(&bits2, &rs2, sizeof(bits2));

		uint32_t result = ((bits2 ^ bits1) & 0x80000000) | (bits1 & 0x7FFFFFFF);

		std::memcpy(&aluFResult, &result, sizeof(aluFResult));

		break;
	}
	case::Instruction::FMVXW: {
		float rs1 = id_ex_reg.rs1FValue;

		std::memcpy(&aluResult, &rs1, sizeof(aluResult));

		break;
	}
	case::Instruction::FMVWX: {
		uint32_t rs1 = id_ex_reg.rs1Value;

		std::memcpy(&aluFResult, &rs1, sizeof(aluFResult));

		break;
	}
	case::Instruction::FEQS: {
		float rs1 = id_ex_reg.rs1FValue;
		float rs2 = id_ex_reg.rs2FValue;
		bool eq = rs1 == rs2;

		aluResult = static_cast<uint32_t>(eq);
		break;
	}
	case::Instruction::FLTS: {
		float rs1 = id_ex_reg.rs1FValue;
		float rs2 = id_ex_reg.rs2FValue;
		bool eq = rs1 < rs2;

		aluResult = static_cast<uint32_t>(eq);
		break;
	}
	case::Instruction::FLES: {
		float rs1 = id_ex_reg.rs1FValue;
		float rs2 = id_ex_reg.rs2FValue;
		bool eq = rs1 <= rs2;

		aluResult = static_cast<uint32_t>(eq);
		break;
	}
	case::Instruction::FCLASSS: {
		float f = id_ex_reg.rs1FValue;
		uint32_t bits{};
		std::memcpy(&bits, &f, sizeof(bits));
		bool negative = bits & 0x80000000;


		if (std::isinf(f) && negative)                              aluResult |= (1 << 0);
		if (std::isnormal(f) && negative)                           aluResult |= (1 << 1);
		if (std::fpclassify(f) == FP_SUBNORMAL && negative)         aluResult |= (1 << 2);
		if (f == 0.0f && negative)                                  aluResult |= (1 << 3);
		if (f == 0.0f && !negative)                                 aluResult |= (1 << 4);
		if (std::fpclassify(f) == FP_SUBNORMAL && !negative)        aluResult |= (1 << 5);
		if (std::isnormal(f) && !negative)                          aluResult |= (1 << 6);
		if (std::isinf(f) && !negative)                             aluResult |= (1 << 7);
		if (std::isnan(f))                                          aluResult |= (1 << 9);

		break;
	}
	case Instruction::EBREAK: {
		halted = true;
		return;
	}
	case Instruction::ECALL: {
		uint32_t command = readReg(17);

		if (command == 1) {
			std::cout << static_cast<int32_t>(readReg(10)) << '\n';
		}
		else if (command == 93) {
			halted = true;
		}
		return;
	}
	case Instruction::UNKNOWN: {
		std::cout << "Unknown Instruction at PC: " << pc << '\n';
		halted = true;
		return;
	}
	}

	if (next_ex_mem_reg.takeBranch) {
		pc = next_ex_mem_reg.branchTarget;
	}

	next_ex_mem_reg.inst = inst;
	next_ex_mem_reg.pc = id_ex_reg.pc;
	next_ex_mem_reg.aluResult = aluResult;
	next_ex_mem_reg.aluFResult = aluFResult;
	next_ex_mem_reg.rs2Value = id_ex_reg.rs2Value;
	next_ex_mem_reg.rs2FValue = id_ex_reg.rs2FValue;
	next_ex_mem_reg.bubble = false;
}

void Core::memoryStage(Memory& mem) {
	if (ex_mem_reg.bubble) {
		next_mem_wb_reg.bubble = true;
		return;
	}

	DecodedInstruction inst = ex_mem_reg.inst;

	uint32_t memRead{};
	float memFRead{};

	switch (inst.name) {
	case::Instruction::LB: {
		memRead = mem.readByte(ex_mem_reg.aluResult);

		if (memRead & 0x80) {
			memRead = memRead | 0xFFFFFF00;
		}

		break;
	}
	case Instruction::LBU: {
		memRead = mem.readByte(ex_mem_reg.aluResult);
		break;
	}
	case Instruction::LH: {
		memRead = mem.readHalfWord(ex_mem_reg.aluResult);

		if (memRead & 0x8000) {
			memRead = memRead | 0xFFFF0000;
		}

		break;
	}
	case Instruction::LHU: {
		memRead = mem.readHalfWord(ex_mem_reg.aluResult);
		break;
	}
	case Instruction::LW: {
		memRead = mem.readWord(ex_mem_reg.aluResult);
		break;
	}
	case Instruction::SB: {
		uint32_t address = ex_mem_reg.aluResult;
		uint32_t value = ex_mem_reg.rs2Value;
		mem.writeByte(address, value);
		break;
	}
	case Instruction::SH: {
		uint32_t address = ex_mem_reg.aluResult;
		uint32_t value = ex_mem_reg.rs2Value;
		mem.writeHalfWord(address, value);
		break;
	}
	case Instruction::SW: {
		uint32_t address = ex_mem_reg.aluResult;
		uint32_t value = ex_mem_reg.rs2Value;
		mem.writeWord(address, value);
		break;
	}
	case Instruction::FLW: {
		uint32_t memory = mem.readWord(ex_mem_reg.aluResult);
		std::memcpy(&memFRead, &memory, sizeof(memFRead));
		break;
	}
	case Instruction::FSW: {
		uint32_t address = ex_mem_reg.aluResult;
		float f = ex_mem_reg.rs2FValue;
		uint32_t value{};
		std::memcpy(&value, &f, sizeof(value));
		mem.writeWord(address, value);
		break;
	}
	default:
		break;
	}

	next_mem_wb_reg.inst = inst;
	next_mem_wb_reg.memRead = memRead;
	next_mem_wb_reg.memFRead = memFRead;
	next_mem_wb_reg.aluResult = ex_mem_reg.aluResult;
	next_mem_wb_reg.aluFResult = ex_mem_reg.aluFResult;
	next_mem_wb_reg.bubble = false;
}

void Core::writeBackStage() {
	if (mem_wb_reg.bubble) {
		return;
	}

	DecodedInstruction inst = mem_wb_reg.inst;

	if (inst.type == InstructionType::S || inst.type == InstructionType::FPS || inst.type == InstructionType::B ||
		inst.type == InstructionType::ENVIRONMENT || inst.type == InstructionType::UNKNOWN) {
		return;
	}

	bool isFloat = (inst.type == InstructionType::FPL || inst.type == InstructionType::FPA || inst.type == InstructionType::FPR1 ||
		inst.type == InstructionType::FMINMAX || inst.type == InstructionType::FPR4 || inst.type == InstructionType::INTCONVFP);

	if (!isFloat && inst.rd == 0) {
		return;
	}


	if (inst.type == InstructionType::LOAD) {
		writeReg(inst.rd, mem_wb_reg.memRead);
	}
	else if (inst.type == InstructionType::FPL) {
		writeFReg(inst.rd, mem_wb_reg.memFRead);
	}
	else if (isFloat) {
		writeFReg(inst.rd, mem_wb_reg.aluFResult);
	}
	else {
		writeReg(inst.rd, mem_wb_reg.aluResult);
	}
}

void Core::run(Memory& mem) {
	registers[2] = 65536;
	while (!halted) {
		writeBackStage();
		memoryStage(mem);
		executeStage(mem);
		decodeStage(mem);
		fetchStage(mem);

		if (next_ex_mem_reg.takeBranch) {;
			next_id_ex_reg = ID_EX{};
			next_ex_mem_reg.takeBranch = false;
		}

		if_id_reg = next_if_id_reg;
		id_ex_reg = next_id_ex_reg;
		ex_mem_reg = next_ex_mem_reg;
		mem_wb_reg = next_mem_wb_reg;
	}
}