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
    logger.DumpFormula(settings.output_file, out);
    if (settings.show_stats)
        stats.Dump(std::cout);
    for (auto &t : threads)
        t.WaitForResult();
    return out;
}

z3::expr FormulaSimplifier::RunSimplifications()
{
    if (settings.simplify_whole_formula)
    {
        ExprSimplifier simplifier(expr.ctx(), true, true);
        logger.Log("Simplifying...");
        expr = simplifier.Simplify(expr);
        expr = RemoveInternal(expr);
        if (settings.dump_bdds)
            logger.DumpFormula("simplified.smt2", expr);
    }

    std::vector<bool> use;
    FindSubformulas(expr, use);
    use.back() = false;

    std::vector<z3::expr> bound;
    int idx = 0;
    LaunchThreads(expr, bound, use, idx);
    assert(idx == (int)use.size());
    assert(bound.empty());
    if (settings.use_whole_under)
        threads.emplace_back(expr, false, bound, true);
    if (settings.use_whole_over)
        threads.emplace_back(expr, true, bound, true);
    logger.Log(std::to_string(threads.size()) + " threads launched");

    while (!std::all_of(threads.begin(), threads.end(), [](const auto &t)
                        { return t.IsFinished(); }) &&
           !time_manager.IsTimeout())
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    Solver::resultComputed = true;

    if (time_manager.IsTimeout())
        logger.Log("Timeout");

    z3::expr res(expr.ctx());

    auto t_curr = threads.begin();
    idx = 0;
    res = Simplify(expr, t_curr, use, idx);
    assert(idx == (int)use.size());

    logger.Log("Getting result from main thread");
    if (settings.use_whole_under)
    {
        auto &tu = *t_curr++;
        auto under = Translate(tu.GetResult(), expr.ctx());
        for (auto &e : under)
            if (!isFalse(e))
                e = expr.ctx().bool_val(true);
        res = simplifyOr(simplifyOr(expr.ctx(), PickResults(under)), res);
        if (tu.IsPrecise() && settings.replace_precise)
            res = under.back();
    }
    if (settings.use_whole_over)
    {
        auto &to = *t_curr++;
        auto over = Translate(to.GetResult(), expr.ctx());
        if (!isTrue(res))
        {
            res = simplifyAnd(simplifyAnd(expr.ctx(), PickResults(over)), res);
            if (to.IsPrecise() && settings.replace_precise)
                res = over.back();
        }
    }
    assert(t_curr == threads.end());

    return res;
}

z3::expr FormulaSimplifier::Simplify(z3::expr e, std::list<SimplifierThread>::iterator &t_curr, const std::vector<bool> &use, int &idx)
{
    if (e.is_const() || !e.is_bool())
    {
        assert(!use[idx]);
        ++idx;
        return e;
    }

    if (e.is_app())
    {
        z3::func_decl f = e.decl();
        unsigned num = e.num_args();

        auto decl_kind = f.decl_kind();

        std::vector<z3::expr> sim;
        for (unsigned i = 0; i < num; ++i)
            sim.push_back(Simplify(e.arg(i), t_curr, use, idx));

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
            e = z3::forall(bound, Simplify(e.body(), t_curr, use, idx));
        else
            e = z3::exists(bound, Simplify(e.body(), t_curr, use, idx));
    }
    else
    {
        assert(!use[idx]);
    }

    if (use[idx])
    {
        logger.Log("Getting result from thread");
        if (settings.use_under)
        {
            auto &tu = *t_curr++;
            auto under = Translate(tu.GetResult(), e.ctx());
            e = simplifyOr(simplifyOr(e.ctx(), PickResults(under)), e);
            if (tu.IsPrecise() && settings.replace_precise)
                e = under.back();
        }
        if (settings.use_over)
        {
            auto &to = *t_curr++;
            auto over = Translate(to.GetResult(), e.ctx());
            e = simplifyAnd(simplifyAnd(e.ctx(), PickResults(over)), e);
            if (to.IsPrecise() && settings.replace_precise)
                e = over.back();
        }
    }

    ++idx;
    return e;
}

void FormulaSimplifier::FindSubformulas(z3::expr e, std::vector<bool> &use)
{
    if (e.is_const() || !e.is_bool())
    {
        use.push_back(false);
        return;
    }

    if (e.is_app())
    {
        std::vector<int> child_idx;
        unsigned num = e.num_args();
        for (unsigned i = 0; i < num; ++i)
        {
            FindSubformulas(e.arg(i), use);
            child_idx.push_back((int)use.size() - 1);
        }
        use.push_back(false);
    }
    else if (e.is_quantifier())
    {
        FindSubformulas(e.body(), use);
        use.back() = false;
        use.push_back(true);
    }
    else
    {
        use.push_back(false);
    }
}

void FormulaSimplifier::LaunchThreads(z3::expr e, std::vector<z3::expr> &bound, const std::vector<bool> &use, int &idx)
{
    if (e.is_const() || !e.is_bool())
    {
        assert(!use[idx]);
        ++idx;
        return;
    }

    if (e.is_app())
    {
        unsigned num = e.num_args();
        for (unsigned i = 0; i < num; ++i)
            LaunchThreads(e.arg(i), bound, use, idx);
    }
    else if (e.is_quantifier())
    {
        auto new_bound = GetQuantBoundVars(e);
        auto curr_size = bound.size();
        for (auto b : new_bound)
            bound.push_back(b);

        LaunchThreads(e.body(), bound, use, idx);
        while (bound.size() > curr_size)
            bound.pop_back();
    }
    else
    {
        assert(!use[idx]);
    }

    if (use[idx])
    {
        if (settings.use_under)
            threads.emplace_back(e, false, bound);
        if (settings.use_over)
            threads.emplace_back(e, true, bound);
    }
    ++idx;
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

std::vector<z3::expr> FormulaSimplifier::PickResults(const std::vector<z3::expr> &approx)
{
    const int n = settings.n_approx_pick;
    if ((int)approx.size() <= n)
        return approx;

    if (n == 1)
        return {approx.back()};

    std::vector<z3::expr> result;
    for (int i = 0; i < n; ++i)
        result.push_back(approx[(approx.size() - 1) * i / (n - 1)]);
    return result;
}
