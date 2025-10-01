#include <iostream>
#include <string>

#include "FBS_SMTVisitor.h"
#include "TimeoutManager.h"
#include "FBSLogger.h"
#include "Settings.h"

#include "antlr4-runtime.h"
#include "SMTLIBv2Lexer.h"
#include "SMTLIBv2Parser.h"

#include "Config.h"
#include "Logger.h"

#include "FormulaSimplifier.h"

#include "ExprToBDDTransformer.h"

using namespace antlr4;

void PrintUsage(const char *argv0)
{
    std::cout << "Usage: " << argv0 << " [options] filename.smt2\n";
    std::cout << "Available options:\n";
    std::cout << "    --verbose:[1/0] prints debug output if 1, default 0\n";
    std::cout << "    --timeout:n timeout in seconds, 0 for no timeout, default 0\n";
    std::cout << "    --use-over:[1/0] whether to use overapproximations, default 1\n";
    std::cout << "    --use-under:[1/0] whether to use underapproximations, default 0\n";
    std::cout << "    --max-quants:n maximum number of quantifiers, 0 for no limit, default 0\n";
    std::cout << "    --bddtof-pattern:[1/0] enable pattern detection in BDD to formula conversion if 1, default 1\n";
    std::cout << "    --dump-bdds:[1/0] save all computed bdds as .dot files if 1, default 0\n";
    std::cout << "    --simplify-whole:[1/0] whether to simplify the whole formula (1) or only the processed subformulas (0), default 0\n";
    std::cout << "    --replace-precise:[1/0] replaces the subformula if the precise BDD is computed if 1, default 1\n";
    std::cout << "    --show-stats:[1/0] shows various statistics from the conversion process if 1, default 0\n";
    std::cout << "    --out:filename the name of the output file, default out.smt2\n";
}

int main(int argc, char **argv)
{
    z3::set_param("pp.max_depth", "4294967295");
    z3::set_param("pp.max_width", "4294967295");
    z3::set_param("pp.max_ribbon", "4294967295");
    z3::set_param("pp.single_line", "true");

    // logger.SetEnabled(true);
    // z3::context c;
    // auto x = c.bv_const("x", 13);
    // auto y = c.bv_const("y", 13);
    // auto a = c.bv_const("a", 13);
    // auto b = c.bv_const("b", 13);
    // auto w = c.bv_const("w", 13);
    // auto d = c.bv_const("d", 13);
    // auto e = (x | y) == 3546;
    // // auto e = z3::forall(y, x * y == c.bv_val(0, 10));
    // // settings.bddtof_pattern = false;
    // logger.DumpFormula("in.smt2", e);
    // SimplifierThread st(e, true, {});
    // st.WaitForResult();
    // logger.DumpFormula("out.smt2", st.GetResult().back());
    // exit(0);

    if (argc < 2)
    {
        PrintUsage(argv[0]);
        return 1;
    }

    for (int i = 1; i < argc - 1; ++i)
    {
        int x = 0;
        char buf[257];
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
        else if (sscanf(argv[i], "--max-quants:%d", &x) == 1)
        {
            settings.max_quants = x;
        }
        else if (sscanf(argv[i], "--bddtof-pattern:%d", &x) == 1 && x >= 0 && x <= 1)
        {
            settings.bddtof_pattern = (bool)x;
        }
        else if (sscanf(argv[i], "--dump-bdds:%d", &x) == 1 && x >= 0 && x <= 1)
        {
            settings.dump_bdds = (bool)x;
        }
        else if (sscanf(argv[i], "--simplify-whole:%d", &x) == 1 && x >= 0 && x <= 1)
        {
            settings.simplify_whole_formula = (bool)x;
        }
        else if (sscanf(argv[i], "--replace-precise:%d", &x) == 1 && x >= 0 && x <= 1)
        {
            settings.replace_precise = (bool)x;
        }
        else if (sscanf(argv[i], "--show-stats:%d", &x) == 1 && x >= 0 && x <= 1)
        {
            settings.show_stats = (bool)x;
        }
        else if (sscanf(argv[i], "--out:%256s", buf) == 1)
        {
            settings.output_file = buf;
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

    SMTLIBv2Parser::StartContext *tree = parser.start();

    Config config;
    FBS_SMTVisitor interpreter;
    interpreter.SetConfig(config);
    interpreter.Run(tree->script());
}
