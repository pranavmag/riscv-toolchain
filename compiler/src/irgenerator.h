# pragma once

#include "ast.h"
#include "visitor.h"
#include "shared/ir.h"
#include <vector>
#include <unordered_map>
#include <string>

// Where a vreg ends up living, once allocated. Today only StackSlot is
// ever produced (naive allocator, everything on the stack) -- PhysReg
// exists so a future linear-scan allocator can hand out real registers
// without Codegen's interface changing at all.
enum class LocationKind {
	StackSlot,
	PhysReg,
};

struct Location {
	LocationKind kind{ LocationKind::StackSlot };
	int value{}; // StackSlot: byte offset from the frame pointer (negative). PhysReg: register number.
};

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

	// filled in by RegisterAllocator::allocate() -- empty/zero until then
	std::unordered_map<int, Location> vregLocations;
	int frameSize{};
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
		// "<entry>" can never collide with a real user identifier: the lexer's
		// identifier() rule only starts a token when isAlpha(c) is true (a
		// letter or '_'), so no user-written name can ever contain '<'. This
		// keeps the implicit top-level program un-nameable and un-callable
		// from user code, and leaves "main" completely free for the user to
		// declare like any other ordinary function name.
		currentFunction_ = createFunction("<entry>");
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

	void allocateRegisters();
	void finalizeFunctions();

	const std::vector<std::unique_ptr<Function>>& getFunctions() const { return functions_; }

	void printIR();
	void printAllocations();
	std::string operandToString(Operand operand);
	std::string operatorToString(IROp operand);
};