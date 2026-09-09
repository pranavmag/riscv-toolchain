# pragma once

#include "lexer.h"
#include "ast.h"
#include <string>
#include <vector>
#include <memory>
#include <initializer_list>

class Parser {
	std::vector<Token> tokens_;
	int current_ = 0;
	ErrorHandling& error_;

public: 
	Parser(const std::vector<Token> tok, ErrorHandling& error) : tokens_(std::move(tok)), error_(error) {}

	std::vector<std::unique_ptr<Stmt>> parseCode();

private:
	// Statements (Recursive Descent Parsing)
	std::unique_ptr<Stmt> statement();
	std::unique_ptr<Stmt> varDeclaration(Token type, Token name);
	std::unique_ptr<Stmt> funcDeclaration(Token returnType, Token name);	std::unique_ptr<Stmt> retStatement();
	std::unique_ptr<Stmt> ifStatement();
	std::unique_ptr<Stmt> whileStatement();
	std::unique_ptr<Stmt> blockStatement();

	// Expressions (Pratt Parsing)
	// 1 + 2 * 3
	// nud(1)
	// led(left, +)
	// int bindingPower(Token)
	std::unique_ptr<Expr> parseExpr(int minBindingPower);
	std::unique_ptr<Expr> nud(Token token);
	std::unique_ptr<Expr> led(Token op, std::unique_ptr<Expr> left);
	int bindingPower(Token token);

	// Helpers
	bool match(std::initializer_list<TokenType> types);
	Token advance();
	bool isAtEnd() const;
	bool check(TokenType type) const;
	Token consume(TokenType type, const std::string& message);
	Token peek() const;
	Token previous() const;
	void synchronize();
};


class ParseError : public std::runtime_error {
public:
	ParseError(const std::string& message) : std::runtime_error(message) {}
};