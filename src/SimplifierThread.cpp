#include <iostream>

#include "SimplifierThread.h"
#include "SimplifierBasic.h"
#include "FBSLogger.h"

#include "Solver.h"

z3::expr Translate(z3::expr e, z3::context& ctx)
{
    auto res = z3::expr(ctx, Z3_translate(e.ctx(), e, ctx));
    return res;
}

std::vector<z3::expr> Translate(const std::vector<z3::expr>& es, z3::context& ctx)
{
    std::vector<z3::expr> res;
    int sz = (int)es.size();
    for (int i = 0; i < sz; ++i)
        res.push_back(Translate(es[i], ctx));
    return res;
}

z3::expr_vector GetQuantBoundVars(z3::expr e)
{
    int bound = Z3_get_quantifier_num_bound(e.ctx(), e);

    z3::expr_vector res(e.ctx());
    for (int i = 0; i < bound; ++i)
    {
        Z3_symbol z3_symbol = Z3_get_quantifier_bound_name(e.ctx(), e, i);
        Z3_sort z3_sort = Z3_get_quantifier_bound_sort(e.ctx(), e, i);

        z3::symbol symbol(e.ctx(), z3_symbol);
        z3::sort sort(e.ctx(), z3_sort);

        res.push_back(e.ctx().constant(symbol, sort));
    }

    return res;
}

void SimplifierThread::Run()
{
    logger.Log("Collecting vars...");
    expr = CollectVars(expr, 0);

    logger.Log("Creating transformer...");
    logger.DumpFormula("h.smt2", expr);
    transformer = std::make_unique<ExprToBDDTransformer>(expr.ctx(), expr, Config());

    logger.Log("Running approximations...");
    RunApprox();

    finished = true;
}

void SimplifierThread::RunApprox()
{
    if (Solver::resultComputed)
        return;
        
    if (!overapproximate)
        transformer->setApproximationType(ZERO_EXTEND);

    int bw = 1;
    int prec = 1;
    std::vector<int> node_counts;
    node_counts.push_back(0);
    while (bw <= 128 && prec <= 16)
    {
        // logger.Log("Running expr to bdd (over = " + std::to_string(overapproximate) +
        //             "; bw = " + std::to_string(bw) + "; prec = " + std::to_string(prec) + ")...");
        BDDInterval bdds;
        if (overapproximate)
            bdds = transformer->ProcessOverapproximation(bw, prec);
        else
            bdds = transformer->ProcessUnderapproximation(bw, prec);
        if (Solver::resultComputed)
            return;
        const auto& bdd = overapproximate ? bdds.upper : bdds.lower;
        //logger.DumpFormulaBDD(expr, bdd.upper);

        if (overapproximate && bdd.IsZero())
        {
            logger.Log("Bdd always false");
            result.clear();
            result.push_back(expr.ctx().bool_val(false));
            return;
        }
        if (!overapproximate && bdd.IsOne())
        {
            logger.Log("Bdd always true");
            result.clear();
            result.push_back(FixUnder(expr.ctx().bool_val(true), bw));
            return;
        }

        int nc = bdd.nodeCount();
        if (nc != node_counts.back() && !bdd.IsZero() && !bdd.IsOne())
        {
            logger.Log("Useful result returned (" + std::to_string(bdd.nodeCount()) + " nodes)");
            auto cand = BDDToFormula(bdd);
            if (!overapproximate)
                cand = FixUnder(cand, bw);
            if (Solver::resultComputed)
                return;
            while (nc < node_counts.back())
            {
                node_counts.pop_back();
                result.pop_back();
            }
            node_counts.push_back(nc);
            result.push_back(cand);
        }

        if (transformer->OperationApproximationHappened())
            prec *= 4;
        else if (bw == 1)
            bw = 2;
        else
            bw += 2;
    }
}

z3::expr SimplifierThread::BDDToFormula(DdNode* node)
{
    if (expr_cache.find(node) != expr_cache.end())
        return expr_cache.at(node);

    z3::expr texpr = BDDToFormula(Cudd_Regular(Cudd_T(node)));
    if (Solver::resultComputed)
        return expr.ctx().bool_val(false);
    z3::expr fexpr = BDDToFormula(Cudd_Regular(Cudd_E(node)));
    if (Solver::resultComputed)
        return expr.ctx().bool_val(false);

    if (Cudd_IsComplement(Cudd_E(node)))
        fexpr = simplifyNot(fexpr);

    int idx = Cudd_NodeReadIndex(node);
    const auto&[name, bit] = idx_to_var.at(idx);
    z3::expr var = vars.at(name);
    if (!var.is_bool())
        var = var.extract(bit, bit) == expr.ctx().bv_val(1, 1);

    z3::expr result = simplifyIte(var, texpr, fexpr);
    expr_cache.emplace(node, result);
    return result;
}

z3::expr SimplifierThread::BDDToFormula(const BDD& bdd)
{
    expr_cache.clear();
    idx_to_var.clear();
    for (const auto&[name, bvec] : transformer->vars)
    {
        for (int i = 0; i < bvec.bitnum(); ++i)
        {
            int idx = bvec[i].GetBDD().NodeReadIndex();
            idx_to_var[idx] = std::make_pair(name, i);
        }
    }
    expr_cache.emplace(Cudd_ReadOne(bdd.manager()), expr.ctx().bool_val(true));
    expr_cache.emplace(Cudd_ReadZero(bdd.manager()), expr.ctx().bool_val(false));

    auto ne = BDDToFormula(bdd.getRegularNode());
    if (Solver::resultComputed)
        return expr.ctx().bool_val(false);

    if (Cudd_IsComplement(bdd.getNode()))
        ne = simplifyNot(ne);

    return ne;
}

z3::expr SimplifierThread::CollectVars(z3::expr e, int n_bound)
{
    if (Solver::resultComputed)
        return e;

    if (e.is_var())
    {
        int idx = Z3_get_index_value(e.ctx(), e);
        if (idx >= n_bound)
        {
            auto res = pre_bound[pre_bound.size() + n_bound - idx - 1];
            vars.emplace(res.to_string(), res);
            std::cout << "Pre bound subst: idx = " << idx << ", var = " << res << '\n';
            return res;
        }
        return e;
    }

    if (e.is_const() && !e.is_numeral())
    {
        vars.emplace(e.to_string(), e);
    }
    
    if (e.is_app())
    {
        z3::func_decl f = e.decl();
        unsigned num = e.num_args();

        auto decl_kind = f.decl_kind();

        z3::expr_vector sim(e.ctx());
        for (unsigned i = 0; i < num; ++i)
            sim.push_back(CollectVars(e.arg(i), n_bound));

        return f(sim);
    }

    if (e.is_quantifier())
    {
        auto bound = GetQuantBoundVars(e);

        if (e.is_forall())
            return z3::forall(bound, CollectVars(e.body(), n_bound + (int)bound.size()));
        return z3::exists(bound, CollectVars(e.body(), n_bound + (int)bound.size()));
    }

    return e;
}

z3::expr SimplifierThread::FixUnder(z3::expr e, int bw)
{
    std::vector<z3::expr> conj;
    for (auto&[n, v] : vars)
    {
        auto sort = v.get_sort();
        if (sort.is_bool())
            continue;
        auto bits = sort.bv_size();
        if (bits <= bw)
            continue;
        int lo_bit = (bw + 1) / 2;
        int hi_bit = bits - 1 - bw / 2;
        conj.push_back(v.extract(hi_bit, lo_bit) == e.ctx().bv_val(0, bits - bw));
    }
    conj.push_back(e);
    return simplifyAnd(e.ctx(), conj);
}
