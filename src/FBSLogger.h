#include <mutex>
#include <string>
#include <chrono>
#include <z3++.h>
#include "ExprToBDDTransformer.h"

class FBSLogger
{
public:
    FBSLogger() { start_tp = std::chrono::high_resolution_clock::now(); }
    
    void Log(const std::string& str);

    void DumpBDD(const BDD& bdd);
    void DumpFormula(const std::string& filename, const z3::expr& expr);

    void DumpFormulaBDD(const z3::expr& expr, const BDD& bdd);

private:
    std::mutex output_mutex;
    int next_dot_id = 0;
    std::chrono::high_resolution_clock::time_point start_tp;

    void LogSafe(const std::string& str);
};

static inline FBSLogger logger;