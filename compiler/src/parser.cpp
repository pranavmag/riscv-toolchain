#include "parser.h"
#include <vector>
#include <string>
#include <memory>

bool Parser::check(TokenType type) const {
	if (!isAtEnd()) {
		return peek().type == type;
	}
	return false;
}

Token Parser::advance() {
	if (!isAtEnd()) {
		return tokens_[current_++];
	}
	return tokens_[current_];
}

bool Parser::isAtEnd() const {
	return peek().type == TokenType::EOF_TOKEN;
}

Token Parser::consume(TokenType type, const std::string& message) {
	if (check(type)) {
		return advance();
	}

	throw ParseError(message);
}

Token Parser::peek() const {
	return tokens_[current_];
}

Token Parser::previous() const {
	return tokens_[current_ - 1];
}

//if (condition {}
void Parser::synchronize() {
	advance();


	while (!isAtEnd()) {
		if (previous().type == TokenType::SEMICOLON) return;

		switch (peek().type) {
		case TokenType::INT: 
		case TokenType::FLOAT:              
		case TokenType::VOID:               
		case TokenType::CHAR:               
		case TokenType::IF:                 
		case TokenType::ELSE:              
		case TokenType::WHILE:              
		case TokenType::FOR:               
		case TokenType::RETURN:             
		case TokenType::PRINT:
			return;

		default: break;
		}

		advance();
	}
}

bool Parser::match(std::initializer_list<TokenType> types) {
	for (TokenType type : types) {
		if (check(type)) {
			advance();
			return true;
		}
	}
	return false;
}

int Parser::bindingPower(Token token) {
	switch (token.type) {
	case TokenType::EQUAL: return 5; // Assignment
	case TokenType::EQUAL_EQUAL:
	case TokenType::EXCLAMATION_EQUAL: return 10; // Equality
	case TokenType::LESS_EQUAL:
	case TokenType::GREATER_EQUAL:
	case TokenType::GREATER:
	case TokenType::LESS: return 20; // Comparison
	case TokenType::PLUS:
	case TokenType::MINUS: return 30; // ADD/SUB
	case TokenType::STAR:
	case TokenType::SLASH:
	case TokenType::PERCENT: return 40; // MUL/DIV/MODULO
	case TokenType::EXCLAMATION: return 50; // Unary
	case TokenType::LEFT_PAREN: return 60; // Parenthesis
	default: return 0;
	}
}

std::unique_ptr<Stmt> Parser::statement() {
	if (match({ TokenType::INT, TokenType::FLOAT, TokenType::VOID, TokenType::CHAR })) {
		Token type = previous();
		Token name = consume(TokenType::IDENTIFIER, "Expected name after type.");
		
		if (check(TokenType::LEFT_PAREN)) {
			return funcDeclaration(type, name);
		}

		return varDeclaration(type, name);
	}
	if (match({ TokenType::RETURN })) return retStatement();
	if (match({ TokenType::IF })) return ifStatement();
	if (match({ TokenType::WHILE })) return whileStatement();
	if (match({ TokenType::LEFT_BRACE })) return blockStatement();

	// expr;
	auto expr = parseExpr(0);
	consume(TokenType::SEMICOLON, "Expected ';' after expression.");
	
	return std::make_unique<ExprStmt>(
		std::move(expr)
	);
}

// return expr;
// return;
std::unique_ptr<Stmt> Parser::retStatement() {
	std::unique_ptr<Expr> value = nullptr;
	if (!check(TokenType::SEMICOLON)) {
		value = parseExpr(0);
	}

	consume(TokenType::SEMICOLON, "Expected ';' after return value.");

	return std::make_unique<RetStmt>(
		std::move(value)
	);
}

// if (condition) {} else {}
std::unique_ptr<Stmt> Parser::ifStatement() {
	consume(TokenType::LEFT_PAREN, "Expected '(' after 'if'.");
	auto condition = parseExpr(0);
	consume(TokenType::RIGHT_PAREN, "Expected ')' after condition.");

	auto branch = statement();

	std::unique_ptr<Stmt> elseBranch = nullptr;
	if (match({ TokenType::ELSE })) {
		elseBranch = statement();
	}

	return std::make_unique<IfStmt>(
		std::move(condition),
		std::move(branch),
		std::move(elseBranch)
	);
}

// while (condition) {}
std::unique_ptr<Stmt> Parser::whileStatement() {
	consume(TokenType::LEFT_PAREN, "Expected '(' after 'if'.");
	auto condition = parseExpr(0);
	consume(TokenType::RIGHT_PAREN, "Expected ')' after condition.");

	auto body = statement();

	return std::make_unique<WhileStmt>(
		std::move(condition),
		std::move(body)
	);
}

// int x = 34; int x;
std::unique_ptr<Stmt> Parser::varDeclaration(Token type, Token name) {
	(void)type;

	std::unique_ptr<Expr> initializer = nullptr;
	if (match({ TokenType::EQUAL })) {
		initializer = parseExpr(0);
	}

	consume(TokenType::SEMICOLON, "Expected ';' after initializer.");

	return std::make_unique<VarDeclStmt>(name, std::move(initializer));
}


// {statement; statement; statement;}
std::unique_ptr<Stmt> Parser::blockStatement() {
	std::vector<std::unique_ptr<Stmt>> stmts;
	while (!check(TokenType::RIGHT_BRACE) && !isAtEnd()) {
		stmts.push_back(statement());
	}

	consume(TokenType::RIGHT_BRACE, "Expected '}' after statements.");

	return std::make_unique<BlockStmt>(
		std::move(stmts)
	);
}

// 1 + 2 * 3   x == y 
std::unique_ptr<Expr> Parser::parseExpr(int minBindingPower) {
	Token tok = advance();
	auto leftNode = nud(tok);

	while (bindingPower(peek()) > minBindingPower) {
		Token op = advance();

		leftNode = led(op, std::move(leftNode));
	}

	return leftNode;
}


std::unique_ptr<Expr> Parser::nud(Token token) {
	// start of expressions, numbers, identifiers, unary
	// !x or -y UnaryOpNode
	// numbers = literalNode
	// identifiers = identifierNode

	switch (token.type) {
		// Literal Values (Numbers, Strings)
	case TokenType::NUMBER:
	case TokenType::STRING:
		return std::make_unique<LiteralNode> (token.literal);

		// Identifiers
	case TokenType::IDENTIFIER:
	case TokenType::PRINT:
		return std::make_unique<IdentifierNode> (token);

	case TokenType::EXCLAMATION:
	case TokenType::MINUS: {
		auto op = parseExpr(100);
		return std::make_unique<UnaryOpNode>(token, std::move(op));
	}
		// Grouped Expressions
		// (1 + 2 * 4) * 3
	case TokenType::LEFT_PAREN: {
		auto expr = parseExpr(0);
		consume(TokenType::RIGHT_PAREN, "Expected ')' after expression.");
		return expr;
	}
	default:
		throw ParseError("Unexpected token in expression.");

	}
}

std::unique_ptr<Expr> Parser::led(Token op, std::unique_ptr<Expr> left) {
	// left to right associativity
	switch (op.type) {
	case TokenType::EQUAL_EQUAL:
	case TokenType::EXCLAMATION_EQUAL:
	case TokenType::LESS_EQUAL:
	case TokenType::GREATER_EQUAL:
	case TokenType::GREATER:
	case TokenType::LESS: 
	case TokenType::PLUS:
	case TokenType::MINUS:
	case TokenType::STAR:
	case TokenType::SLASH:
	case TokenType::PERCENT: {
		// a + b * c - 2
		int bp = bindingPower(op);
		auto right = parseExpr(bp);
		return std::make_unique<BinaryOpNode>(
			std::move(left),
			op,
			std::move(right)
		);
	}

	//right to left associativity
	case TokenType::EQUAL: {
		// x = y = z;
		int bp = bindingPower(op);
		auto right = parseExpr(bp - 1);
		return std::make_unique<BinaryOpNode>(
			std::move(left),
			op,
			std::move(right)
		);
	}

	// function call - functions(arg, arg, arg, ...)
	case TokenType::LEFT_PAREN: {
		std::vector<std::unique_ptr<Expr>> args;
		while (!check(TokenType::RIGHT_PAREN) && !isAtEnd()) {
			args.push_back(parseExpr(0));
			if (!check(TokenType::COMMA)) {
				break;
			}
			advance();
		}

		consume(TokenType::RIGHT_PAREN, "Expected ')' after args.");

		return std::make_unique<FuncCallNode>(
			std::move(left),
			std::move(args)
		);
	}

	default:
		throw ParseError("Unexpected operator in expression.");
	}
}

std::vector<std::unique_ptr<Stmt>> Parser::parseCode() {
	std::vector<std::unique_ptr<Stmt>> stmts;
	while (!isAtEnd()) {
		try {
			stmts.push_back(statement());
		}
		catch (const ParseError& e) {
			error_.error(peek().line, e.what());
			synchronize();
		}
	}

	return stmts;
}

std::unique_ptr<Stmt> Parser::funcDeclaration(Token returnType, Token name) {
	consume(TokenType::LEFT_PAREN, "Expected '(' after function name.");
	std::vector<Param> params;

	if (!check(TokenType::RIGHT_PAREN)) {
		do {
			if (!match({ TokenType::INT, TokenType::FLOAT, TokenType::VOID, TokenType::CHAR })) {
				throw ParseError("Expected parameter type.");
			}
			Token paramType = previous();
			Token paramName = consume(TokenType::IDENTIFIER, "Expected parameter name.");
			params.push_back(Param{ paramType, paramName });
		} while (match({ TokenType::COMMA }));
	}

	consume(TokenType::RIGHT_PAREN, "Expected ')' after parameters.");
	consume(TokenType::LEFT_BRACE, "Expected '{' before function body.");

	auto body = blockStatement();

	return std::make_unique<FuncDeclNode>(returnType, name, std::move(params), std::move(body));
}

