# pragma once

#include "ast.h"
#include "visitor.h"
#include "shared/ir.h"
#include <vector>
#include <unordered_map>
#include <string>

// Everything a single function needs to compile itself in isolation:
// its own CFG, its own vreg/block numbering, and its own scope stack.
// A function must NOT see its caller's locals, so scopes live here,
// not on IRGenerator itself.
struct Function {
	int id{};
	std::string name;

	int vregCounter = 1;
	int blockCounter = 1;

	std::vector<std::unique_ptr<BasicBlock>> cfg;
	BasicBlock* currentBlock = nullptr;

	std::vector<std::unordered_map<std::string, int>> scopes;

	std::vector<int> paramVRegs;
};

class IRGenerator : public ExprVisitor, public StmtVisitor {
private:
	int function_counter_ = 0;

	std::vector<std::unique_ptr<Function>> functions_;
	std::unordered_map<std::string, Function*> functionTable_;

	Function* currentFunction_ = nullptr;

	Function* createFunction(const std::string& name) {
		auto newFunction = std::make_unique<Function>();
		newFunction->id = function_counter_++;
		newFunction->name = name;
		Function* funcPtr = newFunction.get();
		functions_.push_back(std::move(newFunction));
		return funcPtr;
	}

	BasicBlock* createBlock() {
		auto newBlock = std::make_unique<BasicBlock>();
		newBlock->id = currentFunction_->blockCounter++;
		BasicBlock* blockPtr = newBlock.get();
		currentFunction_->cfg.push_back(std::move(newBlock));
		return blockPtr;
	}

	int allocateVReg();
	void emit(Quad& quad);

	void pushScope();
	void popScope();
	void declareVariable(const std::string& name, int reg);
	int lookupVariable(const std::string& name);

public:
	IRGenerator() {
		// the top-level program is itself just a function ("main") --
		// this keeps every other visitor method oblivious to whether
		// it's compiling top-level code or a user-declared function
		currentFunction_ = createFunction("main");
		currentFunction_->currentBlock = createBlock();
		pushScope();
	}

	IROp mapOperator(TokenType type);
	int visitBinaryOpNode(BinaryOpNode& n) override;
	int visitUnaryOpNode(UnaryOpNode& n) override;
	int visitLiteralNode(LiteralNode& n) override;
	int visitIdentifierNode(IdentifierNode& n) override;
	int visitFuncCallNode(FuncCallNode& n) override;
	void visitVarDecl(VarDeclStmt& n) override;
	void visitRet(RetStmt& n) override;
	void visitIf(IfStmt& n) override;
	void visitWhile(WhileStmt& n) override;
	void visitBlock(BlockStmt& n) override;
	void visitExpr(ExprStmt& n) override;
	void visitFuncDecl(FuncDeclNode& n) override;

	void printIR();
	std::string operandToString(Operand operand);
	std::string operatorToString(IROp operand);
};