#include <iostream>
#include <string>

#include "FAS_SMTVisitor.h"

#include "antlr4-runtime.h"
#include "SMTLIBv2Lexer.h"
#include "SMTLIBv2Parser.h"

#include "Config.h"
#include "Logger.h"

#include "FormulaSimplifier.h"

#include "ExprToBDDTransformer.h"

using namespace antlr4;

int main(int argc, char** argv) 
{
    Logger::SetVerbosity(100);
    
    std::string filename;
    if (1 < argc)
    {
	    filename = std::string(argv[1]);
    }
    else
    {
        std::cout << "Filename required" << std::endl;
        return 1;
    }

    std::ifstream stream;
    stream.open(filename);
    if (!stream.good())
    {
        std::cout << "(error \"failed to open file '" << filename << "'\")" << std::endl;
        return 1;
    }

    ANTLRInputStream input(stream);
    SMTLIBv2Lexer lexer(&input);
    CommonTokenStream tokens(&lexer);
    SMTLIBv2Parser parser(&tokens);

    SMTLIBv2Parser::StartContext* tree = parser.start();

    Config config;
    FAS_SMTVisitor interpreter;
    interpreter.SetConfig(config);
    interpreter.Run(tree->script());
}

