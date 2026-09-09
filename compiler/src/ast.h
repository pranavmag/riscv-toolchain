# pragma once

#include "lexer.h"
#include <memory>
#include <vector>

struct ExprVisitor;

struct StmtVisitor;

// Pratt Parsing
struct Expr {
	virtual ~Expr() = default;

	virtual int accept(ExprVisitor& v) = 0;
};

// a + b, a * b, a == b, etc.
struct BinaryOpNode : Expr {
	// left node, right node, operator
	std::unique_ptr<Expr> left;
	std::unique_ptr<Expr> right; 
	Token op;

	BinaryOpNode(std::unique_ptr<Expr> left, Token op, std::unique_ptr<Expr> right)
		: left(std::move(left)), right(std::move(right)), op(std::move(op)) {
	}

	int accept(ExprVisitor& v) override;
};

// !x, -x, etc.
struct UnaryOpNode : Expr {
	Token op;
	std::unique_ptr<Expr> operand;

	UnaryOpNode(Token op, std::unique_ptr<Expr> operand)
		: op(std::move(op)), operand(std::move(operand)) {
	}

	int accept(ExprVisitor& v) override;
};

// 90, 3.14, "hello world", etc
struct LiteralNode : Expr {
	Literal value;

	LiteralNode(Literal value) : value(std::move(value)) {}

	int accept(ExprVisitor& v) override;
};

// variable name
struct IdentifierNode : Expr {
	Token name;

	IdentifierNode(Token name) : name(std::move(name)) {}

	int accept(ExprVisitor& v) override;
};


// func(arg1, arg2, arg3, ...)
struct FuncCallNode : Expr {
	std::unique_ptr<Expr> funcName;
	std::vector<std::unique_ptr<Expr>> args;


	FuncCallNode(std::unique_ptr<Expr> funcName, std::vector<std::unique_ptr<Expr>> args)
		: funcName(std::move(funcName)), args(std::move(args)) {
	}

	int accept(ExprVisitor& v) override;
};


// Recursive Descent Parsing
struct Stmt {
	virtual ~Stmt() = default;

	virtual void accept(StmtVisitor& v) = 0;
};

// int x = expr;
struct VarDeclStmt : Stmt {
	Token name;
	std::unique_ptr<Expr> initializer;

	VarDeclStmt(Token name, std::unique_ptr<Expr> initializer)
		: name(std::move(name)), initializer(std::move(initializer)) {
	}

	void accept(StmtVisitor& v) override;
};

// return expr;
struct RetStmt : Stmt {
	std::unique_ptr<Expr> ret;

	RetStmt(std::unique_ptr<Expr> ret) : ret(std::move(ret)) {}

	void accept(StmtVisitor& v) override;
};

// if (expr) { body }, else is optional
struct IfStmt : Stmt {
	std::unique_ptr<Expr> condition;
	std::unique_ptr<Stmt> branch;
	std::unique_ptr<Stmt> elseBranch; // could be nullptr

	IfStmt(std::unique_ptr<Expr> condition, std::unique_ptr<Stmt> branch, std::unique_ptr<Stmt> elseBranch)
		: condition(std::move(condition)), branch(std::move(branch)), elseBranch(std::move(elseBranch)) {
	}

	void accept(StmtVisitor& v) override;
};

// while (expr) { body }
struct WhileStmt : Stmt {
	std::unique_ptr<Expr> condition;
	std::unique_ptr<Stmt> body;

	WhileStmt(std::unique_ptr<Expr> condition, std::unique_ptr<Stmt> body)
		: condition(std::move(condition)), body(std::move(body)) {
	}

	void accept(StmtVisitor& v) override;
};

// { body }
struct BlockStmt : Stmt {
	std::vector<std::unique_ptr<Stmt>> body;

	BlockStmt(std::vector<std::unique_ptr<Stmt>> body) : body(std::move(body)) {}

	void accept(StmtVisitor& v) override;
};

// expr;
struct ExprStmt : Stmt {
	std::unique_ptr<Expr> expr;

	ExprStmt(std::unique_ptr<Expr> expr) : expr(std::move(expr)) {}

	void accept(StmtVisitor& v) override;
};

struct Param {
	Token type;
	Token name;
};

struct FuncDeclNode : Stmt {
	Token returnType;
	Token name;
	std::vector<Param> params;
	std::unique_ptr<Stmt> body;

	FuncDeclNode(Token returnType, Token name, std::vector<Param> params, std::unique_ptr<Stmt> body)
		: returnType(std::move(returnType)), name(std::move(name)), params(std::move(params)), body(std::move(body)) {
	}

	void accept(StmtVisitor& v) override;
};








