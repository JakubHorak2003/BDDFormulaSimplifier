#include <iostream>
#include <string>

#include "FBS_SMTVisitor.h"
#include "TimeoutManager.h"
#include "FBSLogger.h"

#include "antlr4-runtime.h"
#include "SMTLIBv2Lexer.h"
#include "SMTLIBv2Parser.h"

#include "Config.h"
#include "Logger.h"

#include "FormulaSimplifier.h"

#include "ExprToBDDTransformer.h"

using namespace antlr4;

void PrintUsage(const char* argv0)
{
    std::cout << "Usage: " << argv0 << " [options] filename.smt2\n";
    std::cout << "Available options:\n";
    std::cout << "    --verbose:[1/0] prints debug output if 1, default 0\n";
    std::cout << "    --timeout:n timeout in seconds, 0 for no timeout, default 0\n";
    std::cout << "    --use-over:[1/0] whether to use overapproximations, default 1\n";
    std::cout << "    --use-under:[1/0] whether to use underapproximations, default 0\n";
}

int main(int argc, char** argv) 
{
    if (argc < 2)
    {
        PrintUsage(argv[0]);
        return 1;
    }

    for (int i = 1; i < argc - 1; ++i)
    {
        int x = 0;
        if (sscanf(argv[i], "--verbose:%d", &x) == 1 && x >= 0 && x <= 1)
        {
            logger.SetEnabled(x);
        }
        else if (sscanf(argv[i], "--timeout:%d", &x) == 1 && x >= 0)
        {
            time_manager.SetTimeout(x);
        }
        else if (sscanf(argv[i], "--use-over:%d", &x) == 1 && x >= 0 && x <= 1)
        {
            settings.use_over = (bool)x;
        }
        else if (sscanf(argv[i], "--use-under:%d", &x) == 1 && x >= 0 && x <= 1)
        {
            settings.use_under = (bool)x;
        }
        else
        {
            PrintUsage(argv[0]);
            return 1;
        }
    }

    std::string filename = std::string(argv[argc - 1]);

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
    FBS_SMTVisitor interpreter;
    interpreter.SetConfig(config);
    interpreter.Run(tree->script());
}

