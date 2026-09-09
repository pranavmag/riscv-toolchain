#include "visitor.h"
#include "ast.h"
#include <iostream>
#include <string>

class ASTPrinter : public ExprVisitor, public StmtVisitor {
    int indent_ = 0;

    void print(const std::string& s) {
        std::cout << std::string(indent_ * 2, ' ') << s << '\n';
    }

public:
    int visitBinaryOpNode(BinaryOpNode& n) override {
        print("BinaryOp: " + n.op.lexeme);
        indent_++;
        n.left->accept(*this);
        n.right->accept(*this);
        indent_--;
        return 0;
    }

    int visitUnaryOpNode(UnaryOpNode& n) override {
        print("UnaryOp: " + n.op.lexeme);
        indent_++;
        n.operand->accept(*this);
        indent_--;
        return 0;
    }

    int visitLiteralNode(LiteralNode& n) override {
        print("Literal");
        (void)n;
        return 0;
    }

    int visitIdentifierNode(IdentifierNode& n) override {
        print("Identifier: " + n.name.lexeme);
        return 0;
    }

    int visitFuncCallNode(FuncCallNode& n) override {
        print("FuncCall");
        indent_++;
        n.funcName->accept(*this);
        for (auto& arg : n.args) arg->accept(*this);
        indent_--;
        return 0;
    }

    void visitVarDecl(VarDeclStmt& n) override {
        print("VarDecl: " + n.name.lexeme);
        if (n.initializer) {
            indent_++;
            n.initializer->accept(*this);
            indent_--;
        }
    }

    void visitRet(RetStmt& n) override {
        print("Return");
        if (n.ret) {
            indent_++;
            n.ret->accept(*this);
            indent_--;
        }
    }

    void visitIf(IfStmt& n) override {
        print("If");
        indent_++;
        n.condition->accept(*this);
        n.branch->accept(*this);
        if (n.elseBranch) n.elseBranch->accept(*this);
        indent_--;
    }

    void visitWhile(WhileStmt& n) override {
        print("While");
        indent_++;
        n.condition->accept(*this);
        n.body->accept(*this);
        indent_--;
    }

    void visitBlock(BlockStmt& n) override {
        print("Block");
        indent_++;
        for (auto& s : n.body) s->accept(*this);
        indent_--;
    }

    void visitExpr(ExprStmt& n) override {
        print("ExprStmt");
        indent_++;
        n.expr->accept(*this);
        indent_--;
    }

    void visitFuncDecl(FuncDeclNode& n) override {
	    print("FuncDecl: " + n.name.lexeme);
	    indent_++;
	    n.body->accept(*this);
	    indent_--;
    }
};