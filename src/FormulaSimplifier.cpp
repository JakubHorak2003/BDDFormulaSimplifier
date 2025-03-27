#include <iostream>
#include <thread>
#include <chrono>
#include <future>

#include "FormulaSimplifier.h"
#include "SimplifierThread.h"
#include "SimplifierBasic.h"
#include "FBSLogger.h"

#include "Config.h"
#include "ExprToBDDTransformer.h"
#include "ExprSimplifier.h"
#include "Solver.h"

z3::expr FormulaSimplifier::Run()
{
    // SimplifierThread tr(expr, false);
    // logger.DumpFormula("g.smt2", tr.WaitForResult());
    // return expr;

    ExprSimplifier simplifier(expr.ctx(), true, true);
    logger.Log("Simplifying...");
    expr = simplifier.Simplify(expr);
    logger.DumpFormula("simplified.smt2", expr);
    logger.DumpFormula("out.smt2", expr);
    LaunchThreads(expr);
    logger.Log(std::to_string(threads.size()) + " threads launched");

    if (!threads.empty())
        std::this_thread::sleep_for(std::chrono::seconds(5));
    Solver::resultComputed = true;
    logger.Log("timeout");

    auto t_curr = threads.begin();
    expr = Simplify(expr, t_curr);

    logger.DumpFormula("out.smt2", expr);
    for (auto& t : threads)
        t.WaitForResult();
    return expr;
}

z3::expr FormulaSimplifier::Simplify(z3::expr e, std::list<SimplifierThread>::iterator& t_curr)
{
    ScopedLogger sl(__FUNCTION__);
    if (e.is_const() || !e.is_bool())
    {
        return e;
    }

    if (e.is_app())
    {
        z3::func_decl f = e.decl();
        unsigned num = e.num_args();

        auto decl_kind = f.decl_kind();

        std::vector<z3::expr> sim;
        for (unsigned i = 0; i < num; ++i)
            sim.push_back(Simplify(e.arg(i), t_curr));

        if (decl_kind == Z3_OP_NOT)
        {
            e = simplifyNot(sim[0]);
        }
        else if (decl_kind == Z3_OP_OR)
        {
            e = simplifyOr(e.ctx(), sim);
        }
        else if (decl_kind == Z3_OP_AND)
        {
            e = simplifyAnd(e.ctx(), sim);
        }
        else if (decl_kind == Z3_OP_ITE)
        {
            e = simplifyIte(sim[0], sim[1], sim[2]);
        }
        else if (num > 0)
        {
            e = f(sim.size(), &sim[0]);
        }
    }

    if (e.is_quantifier())
    {
        auto& to = *t_curr++;
        auto& tu = *t_curr++;
        logger.Log("Getting result from thread");
        auto over = Translate(to.GetResult(), e.ctx());
        auto under = Translate(tu.GetResult(), e.ctx());
        e = decorateFormula(e, under, over);
    }

    return e;
}

void FormulaSimplifier::LaunchThreads(z3::expr e)
{
    if (e.is_const() || !e.is_bool())
    {
        return;
    }

    if (e.is_app())
    {
        unsigned num = e.num_args();
        for (unsigned i = 0; i < num; ++i)
            LaunchThreads(e.arg(i));
    }

    if (e.is_quantifier())
    {
        threads.emplace_back(e, true);
        threads.emplace_back(e, false);
    }
}