# pragma once 

#include <vector>

enum class IROp {
	ADD, SUB, MUL, DIV, REM, 
	NEG, NOT,
	LOAD_IMM, MOV,
	PARAM, CALL,
	RET,
	BEQ, JUMP,
	EQ, NE, LT, LE, GT, GE,
	PRINT,
	UNKNOWN,
};

enum class OperandType {
	VREG,
	IMM,
	LABEL,
	FUNC,
	NONE
};

struct Operand {
	OperandType type{ OperandType::NONE };
	int value{};
};

struct Quad {
	Operand dest;
	Operand src1;
	Operand src2;
	IROp op;
};

struct BasicBlock {
	int id;
	std::vector<Quad> instructions;

	BasicBlock* trueEdge{ nullptr };
	BasicBlock* falseEdge{ nullptr };
};