#include "irgenerator.h"
#include <vector>
#include <iostream>
#include <stdexcept>

int IRGenerator::allocateVReg() {
	return currentFunction_->vregCounter++;
}

void IRGenerator::emit(Quad& quad) {
	currentFunction_->currentBlock->instructions.push_back(quad);
}

IROp IRGenerator::mapOperator(TokenType type) {
	switch (type) {
	case TokenType::PLUS: return IROp::ADD;
	case TokenType::MINUS: return IROp::SUB;
	case TokenType::STAR: return IROp::MUL;
	case TokenType::SLASH: return IROp::DIV;
	case TokenType::PERCENT: return IROp::REM;
	case TokenType::EQUAL_EQUAL: return IROp::EQ;
	case TokenType::EXCLAMATION_EQUAL: return IROp::NE;
	case TokenType::LESS: return IROp::LT;
	case TokenType::LESS_EQUAL: return IROp::LE;
	case TokenType::GREATER: return IROp::GT;
	case TokenType::GREATER_EQUAL: return IROp::GE;
	default: return IROp::UNKNOWN;
	}
}

std::string IRGenerator::operandToString(Operand operand) {
	switch (operand.type) {
	case OperandType::VREG: 
		return "v" + std::to_string(operand.value);
	case OperandType::IMM:
		return std::to_string(operand.value);
	case OperandType::LABEL:
		return "L" + std::to_string(operand.value);
	case OperandType::FUNC:
		return "F" + std::to_string(operand.value);
	case OperandType::NONE:
		return "";
	default:
		return "operandToString failure";
	}
}

std::string IRGenerator::operatorToString(IROp operand) {
	switch (operand) {
	case IROp::ADD: return "ADD";
	case IROp::SUB: return "SUB";
	case IROp::MUL: return "MUL";
	case IROp::DIV: return "DIV";
	case IROp::REM: return "REM";
	case IROp::NOT: return "NOT";
	case IROp::NEG: return "NEG";
	case IROp::LOAD_IMM: return "LOAD_IMM";
	case IROp::MOV: return "MOV";
	case IROp::PARAM: return "PARAM";
	case IROp::CALL: return "CALL";
	case IROp::RET: return "RET";
	case IROp::BEQ: return "BEQ";
	case IROp::JUMP: return "JUMP";
	case IROp::EQ: return "EQ";
	case IROp::NE: return "NE";
	case IROp::LT: return "LT";
	case IROp::LE: return "LE";
	case IROp::GT: return "GT";
	case IROp::GE: return "GE";
	default: return "Unknown OP";
	}
}

void IRGenerator::pushScope() {
	currentFunction_->scopes.emplace_back();
}

void IRGenerator::popScope() {
	currentFunction_->scopes.pop_back();
}

void IRGenerator::declareVariable(const std::string& name, int reg) {
	currentFunction_->scopes.back()[name] = reg;
}

int IRGenerator::lookupVariable(const std::string& name) {
	for (auto it = currentFunction_->scopes.rbegin(); it != currentFunction_->scopes.rend(); ++it) {
		auto found = it->find(name);
		if (found != it->end()) {
			return found->second;
		}
	}

	throw std::runtime_error("Variable '" + name + "' does not exist");
}

int IRGenerator::visitBinaryOpNode(BinaryOpNode& n) {
	if (n.op.type == TokenType::EQUAL) {
		auto* target = dynamic_cast<IdentifierNode*>(n.left.get());
		if (!target) {
			throw std::runtime_error("Left-hand side of assignment must be a variable.");
		}

		int varReg = lookupVariable(target->name.lexeme);
		int valueReg = n.right->accept(*this);

		Quad moveQuad;
		moveQuad.dest = { OperandType::VREG, varReg };
		moveQuad.src1 = { OperandType::VREG, valueReg };
		moveQuad.src2 = { OperandType::NONE, 0 };
		moveQuad.op = IROp::MOV;
		emit(moveQuad);

		return varReg;
	}

	int leftReg = n.left->accept(*this);
	int rightReg = n.right->accept(*this);
	int destReg = allocateVReg();
	IROp op = mapOperator(n.op.type);

	Quad quad;
	quad.dest = { OperandType::VREG, destReg };
	quad.src1 = { OperandType::VREG, leftReg };
	quad.src2 = { OperandType::VREG, rightReg };
	quad.op = op;
	emit(quad);

	return destReg;
}

int IRGenerator::visitUnaryOpNode(UnaryOpNode& n) {
	int childReg = n.operand->accept(*this);
	int destReg = allocateVReg();

	IROp op;
	if (n.op.type == TokenType::MINUS) {
		op = IROp::NEG;
	}
	else if (n.op.type == TokenType::EXCLAMATION) {
		op = IROp::NOT;
	}
	else {
		throw std::runtime_error("Unknown Operator");
	}

	Quad quad;
	quad.dest = { OperandType::VREG, destReg };
	quad.src1 = { OperandType::VREG, childReg };
	quad.src2 = { OperandType::NONE, 0 };
	quad.op = op;

	emit(quad);

	return destReg;
}

int IRGenerator::visitLiteralNode(LiteralNode& n) {
	int destReg = allocateVReg();
	int literal = 0;

	if (std::holds_alternative<int>(n.value)) {
		literal = std::get<int>(n.value);
	}
	else if (std::holds_alternative<float>(n.value)) {
		literal = static_cast<int>(std::get<float>(n.value));
	}
	else if (std::holds_alternative<std::string>(n.value)) {
		literal = std::stoi(std::get<std::string>(n.value));
	}

	Quad quad;
	quad.dest = { OperandType::VREG, destReg };
	quad.src1 = { OperandType::IMM, literal };
	quad.src2 = { OperandType::NONE, 0 };
	quad.op = IROp::LOAD_IMM;

	emit(quad);

	return destReg;
}

int IRGenerator::visitIdentifierNode(IdentifierNode& n) {
	return lookupVariable(n.name.lexeme);
}

int IRGenerator::visitFuncCallNode(FuncCallNode& n) {
	auto* funcIdent = dynamic_cast<IdentifierNode*>(n.funcName.get());
	if (!funcIdent) {
		throw std::runtime_error("Function call target must be a function name.");
	}

	auto found = functionTable_.find(funcIdent->name.lexeme);
	if (found == functionTable_.end()) {
		throw std::runtime_error("Function '" + funcIdent->name.lexeme + "' does not exist");
	}
	Function* callee = found->second;

	std::vector<int> arguments;
	for (auto& arg : n.args) {
		int param = arg->accept(*this);
		arguments.push_back(param);
	}

	for (auto& arg : arguments) {
		Quad paramQuad;
		paramQuad.dest = { OperandType::NONE, 0};
		paramQuad.src1 = { OperandType::VREG, arg };
		paramQuad.src2 = { OperandType::NONE, 0 };
		paramQuad.op = IROp::PARAM;
		emit(paramQuad);
	}

	int destReg = allocateVReg();

	Quad callQuad;
	callQuad.dest = { OperandType::VREG, destReg };
	callQuad.src1 = { OperandType::FUNC, callee->id };
	callQuad.src2 = { OperandType::IMM, static_cast<int>(arguments.size()) };
	callQuad.op = IROp::CALL;

	emit(callQuad);

	return destReg;
}

void IRGenerator::visitVarDecl(VarDeclStmt& n) {
	std::string varName = n.name.lexeme;
	int varReg = allocateVReg();

	if (n.initializer) {
		int initReg = n.initializer->accept(*this);

		Quad moveQuad;
		moveQuad.dest = { OperandType::VREG, varReg };
		moveQuad.src1 = { OperandType::VREG, initReg };
		moveQuad.src2 = { OperandType::NONE, 0 };
		moveQuad.op = IROp::MOV;
		emit(moveQuad);
	}

	declareVariable(varName, varReg);
}

void IRGenerator::visitRet(RetStmt& n) {
	int retReg{};

	if (n.ret) {
		retReg = n.ret->accept(*this);
	}

	Quad quad;
	quad.dest = { OperandType::VREG, retReg };
	quad.src1 = { OperandType::NONE, 0 };
	quad.src2 = { OperandType::NONE, 0 };
	quad.op = IROp::RET;

	emit(quad);
}

void IRGenerator::visitIf(IfStmt& n) {
	int condReg = n.condition->accept(*this);
	BasicBlock* branchBlock = currentFunction_->currentBlock;

	BasicBlock* thenBlock = createBlock();
	BasicBlock* mergeBlock = createBlock();

	BasicBlock* elseBlock = nullptr;
	if (n.elseBranch) {
		elseBlock = createBlock();
	}
	else {
		elseBlock = mergeBlock;
	}

	Quad branchQuad;
	branchQuad.op = IROp::BEQ;
	branchQuad.dest = { OperandType::LABEL, elseBlock->id };
	branchQuad.src1 = { OperandType::VREG, condReg };
	branchQuad.src2 = { OperandType::IMM, 0 };
	emit(branchQuad);

	Quad jumpToThen;
	jumpToThen.op = IROp::JUMP;
	jumpToThen.dest = { OperandType::LABEL, thenBlock->id };
	jumpToThen.src1 = { OperandType::NONE, 0 };
	jumpToThen.src2 = { OperandType::NONE, 0 };
	emit(jumpToThen);

	branchBlock->trueEdge = thenBlock;
	branchBlock->falseEdge = elseBlock;

	currentFunction_->currentBlock = thenBlock;
	n.branch->accept(*this);

	Quad jumpQuad;
	jumpQuad.op = IROp::JUMP;
	jumpQuad.dest = { OperandType::LABEL, mergeBlock->id };
	jumpQuad.src1 = { OperandType::NONE, 0 };
	jumpQuad.src2 = { OperandType::NONE, 0 };
	emit(jumpQuad);

	currentFunction_->currentBlock->trueEdge = mergeBlock;

	if (n.elseBranch) {
		currentFunction_->currentBlock = elseBlock;
		n.elseBranch->accept(*this);
		emit(jumpQuad);
		currentFunction_->currentBlock->trueEdge = mergeBlock;
	}

	currentFunction_->currentBlock = mergeBlock;
}

void IRGenerator::visitWhile(WhileStmt& n) {
	BasicBlock* condBlock = createBlock();

	Quad jumpToCond;
	jumpToCond.op = IROp::JUMP;
	jumpToCond.dest = { OperandType::LABEL, condBlock->id };
	jumpToCond.src1 = { OperandType::NONE, 0 };
	jumpToCond.src2 = { OperandType::NONE, 0 };
	emit(jumpToCond);

	currentFunction_->currentBlock->trueEdge = condBlock;

	currentFunction_->currentBlock = condBlock;
	int condReg = n.condition->accept(*this);

	BasicBlock* bodyBlock = createBlock();
	BasicBlock* endBlock  = createBlock();

	Quad branchQuad;
	branchQuad.op = IROp::BEQ;
	branchQuad.dest = { OperandType::LABEL, endBlock->id };
	branchQuad.src1 = { OperandType::VREG, condReg };
	branchQuad.src2 = { OperandType::IMM, 0 };
	emit(branchQuad);

	Quad jumpToBody;
	jumpToBody.op = IROp::JUMP;
	jumpToBody.dest = { OperandType::LABEL, bodyBlock->id };
	jumpToBody.src1 = { OperandType::NONE, 0 };
	jumpToBody.src2 = { OperandType::NONE, 0 };
	emit(jumpToBody);

	currentFunction_->currentBlock->trueEdge = bodyBlock;
	currentFunction_->currentBlock->falseEdge = endBlock;

	currentFunction_->currentBlock = bodyBlock;
	n.body->accept(*this);

	emit(jumpToCond);
	currentFunction_->currentBlock->trueEdge = condBlock;

	currentFunction_->currentBlock = endBlock;
}

void IRGenerator::visitBlock(BlockStmt& n) {
	pushScope();
	for (auto& stmt : n.body) {
		stmt->accept(*this);
	}
	popScope();
}

void IRGenerator::visitExpr(ExprStmt& n) {
	n.expr->accept(*this);
}

void IRGenerator::visitFuncDecl(FuncDeclNode& n) {
	if (functionTable_.count(n.name.lexeme)) {
		throw std::runtime_error("Function '" + n.name.lexeme + "' is already defined.");
	}

	Function* funcPtr = createFunction(n.name.lexeme);
	// register before compiling the body -- lets the function call itself (recursion)
	functionTable_[funcPtr->name] = funcPtr;

	Function* previousFunction = currentFunction_;
	currentFunction_ = funcPtr;
	currentFunction_->currentBlock = createBlock();

	// one scope for params (visible through the whole function),
	// visitBlock will push its own nested scope for the body's own locals
	pushScope();
	for (auto& param : n.params) {
		int paramReg = allocateVReg();
		currentFunction_->paramVRegs.push_back(paramReg);
		declareVariable(param.name.lexeme, paramReg);
	}

	n.body->accept(*this);

	popScope();

	currentFunction_ = previousFunction;
}

void IRGenerator::printIR() {
	for (const auto& func : functions_) {
		std::cout << "function " << func->name << ":\n";
		for (const auto& block : func->cfg) {
			std::cout << "L" << block->id << ":\n";
			for (const auto& quad : block->instructions) {
				std::cout << "  " << operandToString(quad.dest) << " ";
				std::cout << operatorToString(quad.op) << " ";
				std::cout << operandToString(quad.src1) << " ";
				std::cout << operandToString(quad.src2) << " \n";
			}
		}
	}
}