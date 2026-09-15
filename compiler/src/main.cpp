#include "shared/error.h"
#include "lexer.h"
#include "parser.h"
#include "astprinter.h"
#include "irgenerator.h"
#include "codegen.h"
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>

ErrorHandling errorHandler;

void run(std::string_view source, ErrorHandling& errorHandler, IRGenerator& irGen);

void runFile(const std::string& filePath, ErrorHandling& errHandler) {
	std::ifstream file(filePath);
	if (!file) {
		std::cerr << "Could not open file: " << filePath << '\n';
		std::exit(1);
	}

	IRGenerator irGen;

	std::ostringstream buffer;
	buffer << file.rdbuf();
	run(buffer.str(), errHandler, irGen);
	if (errHandler.hasError) {
		std::exit(2);
	}

	irGen.finalizeFunctions();
	irGen.allocateRegisters();

	try {
		Codegen codegen;
		std::string asmOutput = codegen.generate(irGen.getFunctions());

		std::string outPath = filePath + ".s";
		std::ofstream outFile(outPath);
		outFile << asmOutput;
		std::cout << "Wrote assembly to " << outPath << '\n';
	}
	catch (const std::runtime_error& e) {
		std::cerr << "Codegen error: " << e.what() << '\n';
		std::exit(2);
	}
}

void runPrompt(ErrorHandling& errHandler) {
	std::string userInput{};
	IRGenerator irGen;
	while (true) {
		std::cout << "> ";
		if (!std::getline(std::cin >> std::ws, userInput)) {
			break;
		}
		run(userInput, errHandler, irGen);
		errHandler.hasError = false;
	}
}

void run(std::string_view source, ErrorHandling& errorhandling, IRGenerator& irGen) {
	Scanner scanner(source, errorhandling);
	std::vector<Token> tokens = scanner.scanTokens();

	Parser parser(tokens, errorhandling);
	std::vector<std::unique_ptr<Stmt>> code = parser.parseCode();

	try {
		for (auto& stmt : code) {
			stmt->accept(irGen);
		}

		irGen.printIR();
		irGen.allocateRegisters();
		irGen.printAllocations();
	}
	catch (const std::runtime_error& e) {
		std::cerr << e.what() << '\n';
		errorhandling.hasError = true;
	}
}


int main(int argc, char* argv[]) {
	if (argc > 2) {
		std::cerr << "Too many arguments\n";
    }
	else if (argc == 2) {
		runFile(argv[1], errorHandler);
	}
	else {
		runPrompt(errorHandler);
	}
}