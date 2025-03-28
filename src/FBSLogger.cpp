#include <thread>
#include <iostream>

#include "FBSLogger.h"

FBSLogger logger;

void FBSLogger::Log(const std::string& str)
{
    std::scoped_lock(output_mutex);
    LogSafe(str);
}

void FBSLogger::LogSafe(const std::string& str)
{
    if (!enabled)
        return;
    auto tm = std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::high_resolution_clock::now() - start_tp);
    std::cout << "[Thread " << std::this_thread::get_id() << ", " << tm.count() << "]: " << str << std::endl;
}

void FBSLogger::DumpBDD(const BDD& bdd)
{
    std::scoped_lock(output_mutex);
    auto filename = "bdd" + std::to_string(next_dot_id) + ".dot";
    auto file = fopen(filename.c_str(), "w");
    next_dot_id++;
    DdNode* nodes[] = {bdd.getNode()};
    Cudd_DumpDot(bdd.manager(), 1, nodes, nullptr, nullptr, file);
    fclose(file);
    LogSafe(filename + " created");
}

void FBSLogger::DumpFormula(const std::string& filename, const z3::expr& expr)
{
    std::scoped_lock(output_mutex);
    Z3_set_ast_print_mode(expr.ctx(), Z3_PRINT_SMTLIB2_COMPLIANT);  
    std::string smt2_repr = Z3_benchmark_to_smtlib_string(expr.ctx(), "", "BV", "unknown", "", 0, NULL, expr);

    std::ofstream of(filename);
    of << smt2_repr << std::endl;
    LogSafe(filename + " created");
}

void FBSLogger::DumpFormulaBDD(const z3::expr& expr, const BDD& bdd)
{
    auto filename = "f" + std::to_string(next_dot_id) + ".smt2";
    DumpFormula(filename, expr);
    DumpBDD(bdd);
}
