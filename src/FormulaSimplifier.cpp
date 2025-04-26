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
#include "Settings.h"

#include "Config.h"
#include "ExprToBDDTransformer.h"
#include "ExprSimplifier.h"
#include "Solver.h"

z3::expr FormulaSimplifier::Run()
{
    auto out = RunSimplifications();
    logger.DumpFormula("out.smt2", out);
    for (auto& t : threads)
        t.WaitForResult();
    return out;
}

z3::expr FormulaSimplifier::RunSimplifications()
{
    ExprSimplifier simplifier(expr.ctx(), true, true);
    logger.Log("Simplifying...");
    expr = simplifier.Simplify(expr);
    expr = RemoveInternal(expr);
    logger.DumpFormula("simplified.smt2", expr);
    logger.DumpFormula("out.smt2", expr);

    std::vector<int> quant_cnts;
    CountQuantifiers(expr, 0, quant_cnts);
    int depth = 0;
    int total = 0;
    while (total < 10 && depth < (int)quant_cnts.size())
        total += quant_cnts[depth++];
    logger.Log("Using depth = " + std::to_string(depth) + "/" + std::to_string(quant_cnts.size()) + " with " + std::to_string(total) + " total quantifiers");

    std::vector<z3::expr> bound;
    LaunchThreads(expr, depth, bound);
    assert(bound.empty());
    threads.emplace_back(expr, false, bound);
    logger.Log(std::to_string(threads.size()) + " threads launched");

    while (!std::all_of(threads.begin(), threads.end(), [](const auto& t) { return t.IsFinished(); }) && !time_manager.IsTimeout())
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

    Solver::resultComputed = true;
    if (time_manager.IsTimeout())
        logger.Log("Timeout");
    
    z3::expr res(expr.ctx()), expr_o(expr.ctx()), expr_u(expr.ctx());
    
    auto& tu = threads.back();
    logger.Log("Getting result from main thread");
    auto under = Translate(tu.GetResult(), expr.ctx());
    if (!under.empty())
    {
        logger.Log("Solved using under on the whole formula");
        res = expr_u = expr_o = expr.ctx().bool_val(true);
    }
    else
    {
        auto t_curr = threads.begin();
        res = Simplify(expr, depth, t_curr, true, true, 2);
    
        if (settings.use_over && settings.use_under)
        {
            t_curr = threads.begin();
            expr_o = Simplify(expr, depth, t_curr, true, false, 2);
            t_curr = threads.begin();
            expr_u = Simplify(expr, depth, t_curr, false, true, 2);
        }
    
        assert(std::next(t_curr) == threads.end());    
    }    

    if (settings.use_over && settings.use_under)
    {
        logger.DumpFormula("out.smt2", res);
        logger.DumpFormula("out_o.smt2", expr_o);
        logger.DumpFormula("out_u.smt2", expr_u);
    }
    return res;
}

z3::expr FormulaSimplifier::Simplify(z3::expr e, int depth, std::list<SimplifierThread>::iterator& t_curr, bool use_over, bool use_under, int n_approx_pick)
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

        bool uo = decl_kind == Z3_OP_NOT ? use_under : use_over;
        bool uu = decl_kind == Z3_OP_NOT ? use_over : use_under;

        std::vector<z3::expr> sim;
        for (unsigned i = 0; i < num; ++i)
            sim.push_back(Simplify(e.arg(i), depth, t_curr, uo, uu, n_approx_pick));

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
    else if (e.is_quantifier())
    {
        auto bound = GetQuantBoundVars(e);

        if (e.is_forall())
            e = z3::forall(bound, Simplify(e.body(), depth - 1, t_curr, use_over, use_under, n_approx_pick));
        else
            e = z3::exists(bound, Simplify(e.body(), depth - 1, t_curr, use_over, use_under, n_approx_pick));

        if (depth > 0)
        {
            logger.Log("Getting result from thread");
            if (settings.use_under)
            {
                auto& tu = *t_curr++;
                auto under = Translate(tu.GetResult(), e.ctx());
                if (use_under)
                    e = simplifyOr(e.ctx(), {simplifyOr(e.ctx(), PickResults(under, n_approx_pick)), e});
            }
            if (settings.use_over)
            {
                auto& to = *t_curr++;
                auto over = Translate(to.GetResult(), e.ctx());
                if (use_over)
                    e = simplifyAnd(e.ctx(), {simplifyAnd(e.ctx(), PickResults(over, n_approx_pick)), e});
            }
        }
    }

    return e;
}

void FormulaSimplifier::LaunchThreads(z3::expr e, int depth, std::vector<z3::expr>& bound)
{
    if (e.is_const() || !e.is_bool())
    {
        return;
    }

    if (e.is_app())
    {
        unsigned num = e.num_args();
        for (unsigned i = 0; i < num; ++i)
            LaunchThreads(e.arg(i), depth, bound);
    }

    if (e.is_quantifier())
    {
        auto new_bound = GetQuantBoundVars(e);
        auto curr_size = bound.size();
        for (auto b : new_bound)
            bound.push_back(b);

        LaunchThreads(e.body(), depth - 1, bound);
        while (bound.size() > curr_size)
            bound.pop_back();

        if (depth > 0)
        {
            if (settings.use_under)
                threads.emplace_back(e, false, bound);
            if (settings.use_over)
                threads.emplace_back(e, true, bound);
        }
    }
}

void FormulaSimplifier::CountQuantifiers(z3::expr e, int depth, std::vector<int> &res)
{
    if (e.is_const() || !e.is_bool())
    {
        return;
    }

    if (e.is_app())
    {
        unsigned num = e.num_args();
        for (unsigned i = 0; i < num; ++i)
            CountQuantifiers(e.arg(i), depth, res);
    }

    if (e.is_quantifier())
    {
        if (depth >= (int)res.size())
            res.resize(depth + 1);
        ++res[depth];
        CountQuantifiers(e.body(), depth + 1, res);
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

std::vector<z3::expr> FormulaSimplifier::PickResults(const std::vector<z3::expr> &approx, int n)
{
    if ((int)approx.size() <= n)
        return approx;

    if (n == 1)
        return {approx.back()};

    std::vector<z3::expr> result;
    for (int i = 0; i < n; ++i)
        result.push_back(approx[(approx.size() - 1) * i / (n - 1)]);
    return result;
}
