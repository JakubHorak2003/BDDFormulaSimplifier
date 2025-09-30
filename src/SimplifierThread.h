#pragma once
#include <utility>
#include <map>
#include <thread>
#include <memory>
#include <z3++.h>
#include "ExprToBDDTransformer.h"
#include "Config.h"
#include "FBSLogger.h"
#include "ExprManip.h"

z3::expr Translate(z3::expr e, z3::context &ctx);
std::vector<z3::expr> Translate(const std::vector<z3::expr> &es, z3::context &ctx);
z3::expr_vector GetQuantBoundVars(z3::expr e);

z3::expr RemoveInternal(z3::expr e);

using BDDNode = std::pair<DdNode *, bool>;

class SimplifierThread
{
public:
    SimplifierThread(SimplifierThread &&) = default;
    SimplifierThread(z3::expr e, bool over, const std::vector<z3::expr> &bnd, bool whole = false) : overapproximate(over), whole_formula(whole), expr(Translate(e, ctx)), pre_bound(Translate(bnd, ctx)), thread([this]
                                                                                                                                                                                                                 { Run(); })
    {
    }

    void Run();
    void RunApprox();

    void WaitForResult() { thread.join(); }

    std::string GetThreadId() const;

    bool IsFinished() const { return finished; }
    bool IsPrecise() const { return precise; }

    const std::vector<z3::expr> &GetResult() const { return result; }

private:
    bool overapproximate;
    bool whole_formula;
    z3::context ctx;
    z3::expr expr;
    std::vector<z3::expr> pre_bound;
    std::vector<z3::expr> result;
    bool finished = false;
    bool precise = false;

    std::unique_ptr<ExprToBDDTransformer> transformer;

    int nodes = 0;

    std::map<std::string, z3::expr> vars;
    std::map<BDDNode, z3::expr> expr_cache;
    std::map<int, std::pair<std::string, int>> idx_to_var;

    std::thread thread;

    z3::expr BDDToFormulaWithPatterns(const BDDNode &node);
    z3::expr BDDToFormula(const BDDNode &node);
    z3::expr BDDToFormula(const BDD &bdd);

    z3::expr CollectVars(z3::expr e, int n_bound);

    z3::expr FixUnder(z3::expr e, int bw);

    VarRange GetNodeVar(DdNode *node);
};