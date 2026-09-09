#include "visitor.h"

// Expressions
int BinaryOpNode::accept(ExprVisitor& v) {
	return v.visitBinaryOpNode(*this);
}

int UnaryOpNode::accept(ExprVisitor& v) {
	return v.visitUnaryOpNode(*this);
}

int LiteralNode::accept(ExprVisitor& v) {
	return v.visitLiteralNode(*this);
}

int IdentifierNode::accept(ExprVisitor& v) {
	return v.visitIdentifierNode(*this);
}

int FuncCallNode::accept(ExprVisitor& v) {
	return v.visitFuncCallNode(*this);
}


// Statements
void VarDeclStmt::accept(StmtVisitor& v) {
	v.visitVarDecl(*this);
}

void RetStmt::accept(StmtVisitor& v) {
	v.visitRet(*this);
}

void IfStmt::accept(StmtVisitor& v) {
	v.visitIf(*this);
}

void WhileStmt::accept(StmtVisitor& v) {
	v.visitWhile(*this);
}

void BlockStmt::accept(StmtVisitor& v) {
	v.visitBlock(*this);
}

void ExprStmt::accept(StmtVisitor& v) {
	v.visitExpr(*this);
}

void FuncDeclNode::accept(StmtVisitor& v) {
	v.visitFuncDecl(*this);
}







