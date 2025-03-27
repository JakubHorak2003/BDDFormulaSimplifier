#pragma once
#include <map>
#include <thread>
#include <z3++.h>
#include "ExprToBDDTransformer.h"
#include "Config.h"

z3::expr Translate(z3::expr e, z3::context& ctx);

class SimplifierThread
{
public:
    SimplifierThread(SimplifierThread&&) = default;
    SimplifierThread(z3::expr e, bool over) : overapproximate(over), expr(Translate(e, ctx)), result(ctx.bool_val(over)), transformer(expr.ctx(), expr, Config()), thread([this] { Run(); }) {}

    void Run();

    void RunOver();
    void RunUnder();

    z3::expr WaitForResult() { thread.join(); return result; }

    z3::expr BDDToFormula(DdNode* node);
    z3::expr BDDToFormula(const BDD& bdd);

    void CollectVars(z3::expr e);

    z3::expr GetResult() const { return result; }

    z3::expr FixUnder(z3::expr e, int bw);

private:
    bool overapproximate;
    z3::context ctx;
    z3::expr expr;
    z3::expr result;

    ExprToBDDTransformer transformer;
    std::thread thread;

    int nodes = 0;

    std::map<std::string, z3::expr> vars;
    std::map<const DdNode*, z3::expr> expr_cache;
    std::map<int, std::pair<std::string, int>> idx_to_var;
};