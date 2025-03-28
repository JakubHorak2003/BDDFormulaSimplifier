#include <iostream>
#include <thread>
#include <chrono>
#include <future>
#include <algorithm>
#include <cassert>

#include "FormulaSimplifier.h"
#include "SimplifierThread.h"
#include "SimplifierBasic.h"
#include "FBSLogger.h"
#include "TimeoutManager.h"

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
    expr = RemoveInternal(expr);
    logger.DumpFormula("simplified.smt2", expr);
    logger.DumpFormula("out.smt2", expr);

    std::vector<z3::expr> bound;
    LaunchThreads(expr, bound);
    assert(bound.empty());
    threads.emplace_back(expr, true, bound);
    threads.emplace_back(expr, false, bound);
    logger.Log(std::to_string(threads.size()) + " threads launched");

    while (!std::all_of(threads.begin(), threads.end(), [](const auto& t) { return t.IsFinished(); }) && !time_manager.IsTimeout())
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

    Solver::resultComputed = true;
    if (time_manager.IsTimeout())
        logger.Log("Timeout");

    auto t_curr = threads.begin();
    expr = Simplify(expr, t_curr);

    auto& to = *t_curr++;
    auto& tu = *t_curr++;
    logger.Log("Getting result from main thread");
    auto over = Translate(to.GetResult(), expr.ctx());
    auto under = Translate(tu.GetResult(), expr.ctx());
    if (!under.empty())
    {
        logger.Log("Solved using under on the whole formula");
        expr = expr.ctx().bool_val(true);
    }
    else
    {
        expr = decorateFormula(expr, simplifyOr(expr.ctx(), PickResults(under)), simplifyAnd(expr.ctx(), PickResults(over)));
    }

    logger.DumpFormula("out.smt2", expr);
    for (auto& t : threads)
        t.WaitForResult();
    return expr;
}

z3::expr FormulaSimplifier::Simplify(z3::expr e, std::list<SimplifierThread>::iterator& t_curr)
{
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
        auto bound = GetQuantBoundVars(e);

        if (e.is_forall())
            e = z3::forall(bound, Simplify(e.body(), t_curr));
        else
            e = z3::exists(bound, Simplify(e.body(), t_curr));

        auto& to = *t_curr++;
        auto& tu = *t_curr++;
        logger.Log("Getting result from thread");
        auto over = Translate(to.GetResult(), e.ctx());
        auto under = Translate(tu.GetResult(), e.ctx());
        e = decorateFormula(e, simplifyOr(e.ctx(), PickResults(under)), simplifyAnd(e.ctx(), PickResults(over)));
    }

    return e;
}

void FormulaSimplifier::LaunchThreads(z3::expr e, std::vector<z3::expr>& bound)
{
    if (e.is_const() || !e.is_bool())
    {
        return;
    }

    if (e.is_app())
    {
        unsigned num = e.num_args();
        for (unsigned i = 0; i < num; ++i)
            LaunchThreads(e.arg(i), bound);
    }

    if (e.is_quantifier())
    {
        auto new_bound = GetQuantBoundVars(e);
        auto curr_size = bound.size();
        for (auto b : new_bound)
            bound.push_back(b);

        LaunchThreads(e.body(), bound);
        while (bound.size() > curr_size)
            bound.pop_back();

        threads.emplace_back(e, true, bound);
        threads.emplace_back(e, false, bound);
    }
}

z3::expr FormulaSimplifier::RemoveInternal(z3::expr e)
{
    if (e.is_app())
    {
        z3::func_decl f = e.decl();
        unsigned num = e.num_args();

        auto decl_kind = f.decl_kind();

        z3::expr_vector sim(e.ctx());
        for (unsigned i = 0; i < num; ++i)
            sim.push_back(RemoveInternal(e.arg(i)));

        if (decl_kind == Z3_OP_BSDIV_I)
            return sim[0] / sim[1];
        if (decl_kind == Z3_OP_BSREM_I)
            return z3::srem(sim[0], sim[1]);
        if (decl_kind == Z3_OP_BSMOD_I)
            return z3::smod(sim[0], sim[1]);
        if (decl_kind == Z3_OP_BUDIV_I)
            return z3::udiv(sim[0], sim[1]);
        if (decl_kind == Z3_OP_BUREM_I)
            return z3::urem(sim[0], sim[1]);

        return f(sim);
    }

    if (e.is_quantifier())
    {
        auto bound = GetQuantBoundVars(e);

        if (e.is_forall())
            return z3::forall(bound, RemoveInternal(e.body()));
        return z3::exists(bound, RemoveInternal(e.body()));
    }

    return e;
}

std::vector<z3::expr> FormulaSimplifier::PickResults(const std::vector<z3::expr> &approx)
{
    if (approx.size() <= 1)
        return approx;
    return {approx[0], approx.back()};
}
