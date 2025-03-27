#include <iostream>

#include "SimplifierThread.h"
#include "SimplifierBasic.h"
#include "FBSLogger.h"

#include "Solver.h"

z3::expr Translate(z3::expr e, z3::context& ctx)
{
    return z3::expr(ctx, Z3_translate(e.ctx(), e, ctx));
}

void SimplifierThread::Run()
{
    logger.Log("Collecting vars...");
    CollectVars(expr);
    logger.Log("Collecting vars finished");

    if (overapproximate)
        RunOver();
    else
        RunUnder();
}

void SimplifierThread::RunOver()
{
    int bw = 1;
    int prec = 1;
    while (bw <= 128)
    {
        logger.Log("Running expr to bdd (over; bw = " + std::to_string(bw) + "; prec = " + std::to_string(prec) + ")...");
        auto bdds = transformer.ProcessOverapproximation(bw, prec);
        if (Solver::resultComputed)
            return;
        //logger.DumpFormulaBDD(expr, bdds.upper);

        if (bdds.upper.IsZero())
        {
            logger.Log("Bdd always false");
            result = expr.ctx().bool_val(false);
            return;
        }
        if (!bdds.upper.IsOne())
        {
            logger.Log("Useful result returned (" + std::to_string(bdds.upper.nodeCount()) + " nodes)");
        }
        auto cand = BDDToFormula(bdds.upper);
        if (Solver::resultComputed)
            return;
        result = cand;

        if (transformer.OperationApproximationHappened())
            prec *= 4;
        else if (bw == 1)
            bw = 2;
        else
            bw += 2;
    }
}

void SimplifierThread::RunUnder()
{
    transformer.setApproximationType(ZERO_EXTEND);

    int bw = 1;
    int prec = 1;
    while (bw <= 128)
    {
        logger.Log("Running expr to bdd (under; bw = " + std::to_string(bw) + "; prec = " + std::to_string(prec) + ")...");
        auto bdds = transformer.ProcessUnderapproximation(bw, prec);
        if (Solver::resultComputed)
            return;
        //logger.DumpFormulaBDD(expr, bdds.lower);

        if (bdds.lower.IsOne())
        {
            logger.Log("Bdd always true");
            result = FixUnder(expr.ctx().bool_val(true), bw);
            return;
        }
        if (!bdds.lower.IsZero())
        {
            logger.Log("Useful result returned (" + std::to_string(bdds.lower.nodeCount()) + " nodes)");
        }
        auto cand = FixUnder(BDDToFormula(bdds.lower), bw);
        if (Solver::resultComputed)
            return;
        result = cand;

        if (transformer.OperationApproximationHappened())
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
    for (const auto&[name, bvec] : transformer.vars)
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

void SimplifierThread::CollectVars(z3::expr e)
{
    if (Solver::resultComputed)
        return;

    if (e.is_const() && !e.is_numeral())
    {
        vars.emplace(e.to_string(), e);
    }
    else if (e.is_app())
    {
        for (unsigned i = 0; i < e.num_args(); ++i)
            CollectVars(e.arg(i));
    }
    else if (e.is_quantifier())
    {
        CollectVars(e.body());
    }
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
