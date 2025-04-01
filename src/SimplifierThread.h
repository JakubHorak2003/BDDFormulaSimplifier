#pragma once
#include <map>
#include <thread>
#include <memory>
#include <z3++.h>
#include "ExprToBDDTransformer.h"
#include "Config.h"
#include "FBSLogger.h"

z3::expr Translate(z3::expr e, z3::context& ctx);
std::vector<z3::expr> Translate(const std::vector<z3::expr>& es, z3::context& ctx);
z3::expr_vector GetQuantBoundVars(z3::expr e);

struct ApproxDNF
{
    ApproxDNF(z3::context& c) : ctx(&c) {}

    z3::context* ctx;
    std::vector<std::vector<z3::expr>> clauses;

    z3::expr ToFormula() const;

    void AddConstraint(z3::expr e);

    ApproxDNF MergeWith(const ApproxDNF& other, std::size_t max_size);
};

struct ApproxExpr
{
    ApproxExpr(z3::context& c) : pths_one(c), pths_zero(c) {}

    ApproxDNF pths_one;
    ApproxDNF pths_zero;
};

class SimplifierThread
{
public:
    SimplifierThread(SimplifierThread&&) = default;
    SimplifierThread(z3::expr e, bool over, const std::vector<z3::expr>& bnd) : overapproximate(over), expr(Translate(e, ctx)), pre_bound(Translate(bnd, ctx)), thread([this] { Run(); }) 
    {
    }

    void Run();
    void RunApprox();

    void WaitForResult() { thread.join(); }

    bool IsFinished() const { return finished; }

    z3::expr BDDToFormula(DdNode* node);
    z3::expr BDDToFormula(const BDD& bdd);

    ApproxExpr BDDToFormulaApprox(DdNode* node, std::size_t max_size);
    z3::expr BDDToFormulaApprox(const BDD& bdd, std::size_t max_size);

    z3::expr CollectVars(z3::expr e, int n_bound);

    const std::vector<z3::expr>& GetResult() const { return result; }

    z3::expr FixUnder(z3::expr e, int bw);

private:
    bool overapproximate;
    z3::context ctx;
    z3::expr expr;
    std::vector<z3::expr> pre_bound;
    std::vector<z3::expr> result;
    bool finished = false;

    std::unique_ptr<ExprToBDDTransformer> transformer;
    std::thread thread;

    int nodes = 0;

    std::map<std::string, z3::expr> vars;
    std::map<const DdNode*, z3::expr> expr_cache;
    std::map<const DdNode*, ApproxExpr> approx_expr_cache;
    std::map<int, std::pair<std::string, int>> idx_to_var;
};