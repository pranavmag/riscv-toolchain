# pragma once

#include "ast.h"

struct ExprVisitor {
	virtual ~ExprVisitor() = default;

	virtual int visitBinaryOpNode(BinaryOpNode& n) = 0;
	virtual int visitUnaryOpNode(UnaryOpNode& n) = 0;
	virtual int visitLiteralNode(LiteralNode& n) = 0;
	virtual int visitIdentifierNode(IdentifierNode& n) = 0;
	virtual int visitFuncCallNode(FuncCallNode& n) = 0;

};

struct StmtVisitor {
	virtual ~StmtVisitor() = default;

	virtual void visitVarDecl(VarDeclStmt& n) = 0;
	virtual void visitRet(RetStmt& n) = 0;
	virtual void visitIf(IfStmt& n) = 0;
	virtual void visitWhile(WhileStmt& n) = 0;
	virtual void visitBlock(BlockStmt& n) = 0;
	virtual void visitExpr(ExprStmt& n) = 0;
	virtual void visitFuncDecl(FuncDeclNode& n) = 0;
};

