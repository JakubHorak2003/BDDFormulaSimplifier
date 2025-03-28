#pragma once
#include <mutex>
#include <string>
#include <chrono>
#include <z3++.h>
#include "ExprToBDDTransformer.h"

class DbgScopedLogger
{
public:
    using clck = std::chrono::high_resolution_clock;

    DbgScopedLogger(const std::string& n) : name(n)
    {
        start_tp = clck::now();
        std::cout << "Enter " << name << '\n';
    }

    ~DbgScopedLogger()
    {
        auto dur = std::chrono::duration_cast<std::chrono::duration<double>>(clck::now() - start_tp).count();
        std::cout << "Leave " << name << " [" << dur << "]\n";
    }

private:
    std::string name;
    clck::time_point start_tp;
};

class FBSLogger
{
public:
    FBSLogger() { start_tp = std::chrono::high_resolution_clock::now(); }
    
    void Log(const std::string& str);

    void DumpBDD(const BDD& bdd);
    void DumpFormula(const std::string& filename, const z3::expr& expr);

    void DumpFormulaBDD(const z3::expr& expr, const BDD& bdd);

    void SetEnabled(bool e) { enabled = e; }

private:
    std::mutex output_mutex;
    bool enabled = false;
    int next_dot_id = 0;
    std::chrono::high_resolution_clock::time_point start_tp;

    void LogSafe(const std::string& str);
};

extern FBSLogger logger;