#include <iostream>
#include <sstream>

#include "SimplifierThread.h"
#include "SimplifierBasic.h"
#include "FBSLogger.h"
#include "Settings.h"
#include "Pattern.h"

#include "Solver.h"
#include "ExprSimplifier.h"

z3::expr Translate(z3::expr e, z3::context &ctx)
{
    auto res = z3::expr(ctx, Z3_translate(e.ctx(), e, ctx));
    return res;
}

std::vector<z3::expr> Translate(const std::vector<z3::expr> &es, z3::context &ctx)
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

z3::expr RemoveInternal(z3::expr e)
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

std::string SimplifierThread::GetThreadId() const
{
    std::ostringstream ostr;
    ostr << thread.get_id();
    return ostr.str();
}

void SimplifierThread::Run()
{
    expr = CollectVars(expr, 0);

    if (!settings.simplify_whole_formula)
    {
        ExprSimplifier simplifier(expr.ctx(), true, true);
        logger.Log("Simplifying...");
        if (settings.dump_bdds)
            logger.DumpFormula("in" + GetThreadId() + ".smt2", expr);
        auto simplified = simplifier.Simplify(expr, !whole_formula);
        simplified = RemoveInternal(simplified);
        if (settings.dump_bdds)
        {
            logger.DumpFormula("simplified" + GetThreadId() + ".smt2", simplified);
            logger.DumpFormula("assertstrong" + GetThreadId() + ".smt2", expr && !simplified);
            logger.DumpFormula("assertweak" + GetThreadId() + ".smt2", !expr && simplified);
        }
        expr = simplified;
        logger.Log("Simplifying finished");
    }

    try
    {
        RunApprox();
    }
    catch (const std::logic_error &e)
    {
        logger.Log("Terminated");
    }

    finished = true;
}

void SimplifierThread::RunApprox()
{
    if (Solver::resultComputed)
        return;

    ++stats.stats[Stats::SUBF_CNT];

    if (!overapproximate)
        transformer->setApproximationType(ZERO_EXTEND);

    std::string approx_str = overapproximate ? "over" : "under";

    int bw = 1;
    int prec = 1;
    int last_n_nodes = 0;
    bool fresh_bw = true;

    transformer = std::make_unique<ExprToBDDTransformer>(expr.ctx(), expr, Config());
    while (bw <= 128)
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
        const auto &bdd = overapproximate ? bdds.upper : bdds.lower;
        // logger.DumpFormulaBDD(expr, bdd.upper);

        if (overapproximate && bdd.IsZero())
        {
            logger.Log("Bdd always false " + approx_str + " " + std::to_string(bw) + " " + std::to_string(prec));
            result.clear();
            result.push_back(expr.ctx().bool_val(false));
            ++stats.stats[Stats::SUBF_CONST];
            return;
        }
        if (!overapproximate && bdd.IsOne())
        {
            logger.Log("Bdd always true " + approx_str + " " + std::to_string(bw) + " " + std::to_string(prec));
            result.clear();
            auto cand = FixUnder(expr.ctx().bool_val(true), bw);
            assert(!isFalse(cand));
            result.push_back(cand);
            ++stats.stats[Stats::SUBF_CONST];
            return;
        }

        if (bdd.nodeCount() != last_n_nodes)
        {
            logger.Log("Useful result returned (" + std::to_string(bdd.nodeCount()) + " nodes) " + approx_str + " " + std::to_string(bw) + " " + std::to_string(prec));
            // logger.DumpBDD(bdd);
            auto cand = BDDToFormula(bdd);
            // auto cand = BDDToFormulaApprox(bdd, 5);
            if (settings.dump_bdds)
                logger.DumpFormulaBDD(cand, bdd);
            if (!overapproximate)
                cand = FixUnder(cand, bw);
            if (Solver::resultComputed)
                return;
            last_n_nodes = bdd.nodeCount();
            result.push_back(cand);
        }

        if (transformer->IsPreciseResult() && fresh_bw)
        {
            logger.Log("Precise result returned");
            precise = true;
            ++stats.stats[Stats::SUBF_PREC];
            break;
        }

        int last_bw = bw;
        if (transformer->OperationApproximationHappened())
            prec *= 4;
        else if (bw == 1)
            bw = 2;
        else
            bw += 2;
        fresh_bw = bw > last_bw;
    }

    logger.Log("Done");
}

BDDNode GetNodeChild(const BDDNode &node, bool branch)
{
    if (branch)
        return {Cudd_T(node.first), node.second};
    if (Cudd_IsComplement(Cudd_E(node.first)))
        return {Cudd_Regular(Cudd_E(node.first)), !node.second};
    return {Cudd_E(node.first), node.second};
}

z3::expr SimplifierThread::BDDToFormula(const BDDNode &node)
{
    if (Solver::resultComputed)
        return expr.ctx().bool_val(false);

    if (expr_cache.find(node) != expr_cache.end())
        return expr_cache.at(node);

    ++stats.stats[Stats::NODES_CNT];

    auto var = GetNodeVar(node.first);
    auto tchild = GetNodeChild(node, true);
    auto fchild = GetNodeChild(node, false);
    auto texpr = BDDToFormula(tchild);
    auto fexpr = BDDToFormula(fchild);

    if (var.var.is_bool())
    {
        auto result = simplifyIte(var.var, texpr, fexpr);
        expr_cache.emplace(node, result);
        return result;
    }

    EqNumeral cnt(var, NumeralVal("1"));
    EqNumeral cnf(var, NumeralVal("0"));
    auto result = simplifyIte(cnt.ToExpr(), texpr, fexpr, cnf.ToExpr());

    assert((!isOr(result) && !isAnd(result)) || result.num_args() == 2);
    expr_cache.emplace(node, result);
    return result;
}

z3::expr SimplifierThread::BDDToFormulaWithPatterns(const BDDNode &node)
{
    if (Solver::resultComputed)
        return expr.ctx().bool_val(false);

    if (expr_cache.find(node) != expr_cache.end())
        return expr_cache.at(node);

    ++stats.stats[Stats::NODES_CNT];

    auto var = GetNodeVar(node.first);
    auto tchild = GetNodeChild(node, true);
    auto fchild = GetNodeChild(node, false);

    if (var.var.is_bool())
    {
        auto texpr = BDDToFormulaWithPatterns(tchild);
        auto fexpr = BDDToFormulaWithPatterns(fchild);
        auto result = simplifyIte(var.var, texpr, fexpr);
        expr_cache.emplace(node, result);
        return result;
    }

    auto tvar = GetNodeVar(tchild.first);
    auto fvar = GetNodeVar(fchild.first);
    z3::expr result(expr.ctx());
    if (tvar == fvar && !tvar.IsInvalid() && GetNodeChild(tchild, true) == GetNodeChild(fchild, false) && GetNodeChild(tchild, false) == GetNodeChild(fchild, true))
    {
        ++stats.stats[Stats::NODES_BASE_EQVAR];
        EqVar cv(var, tvar);
        auto texpr = BDDToFormulaWithPatterns(GetNodeChild(tchild, true));
        auto fexpr = BDDToFormulaWithPatterns(GetNodeChild(tchild, false));
        result = MergeEq(cv, texpr, fexpr);
    }
    else if (!fvar.IsInvalid() && fvar.var.to_string() != var.var.to_string() && GetNodeChild(fchild, false) == tchild)
    {
        ++stats.stats[Stats::NODES_BASE_INEQ];
        auto texpr = BDDToFormulaWithPatterns(tchild);
        auto fexpr = BDDToFormulaWithPatterns(GetNodeChild(fchild, true));
        IneqVar iv(fvar, var, true);
        result = MergeIneq(iv, texpr, fexpr);
    }
    else if (!tvar.IsInvalid() && tvar.var.to_string() != var.var.to_string() && GetNodeChild(tchild, true) == fchild)
    {
        ++stats.stats[Stats::NODES_BASE_INEQ];
        auto texpr = BDDToFormulaWithPatterns(fchild);
        auto fexpr = BDDToFormulaWithPatterns(GetNodeChild(tchild, false));
        IneqVar iv(var, tvar, true);
        result = MergeIneq(iv, texpr, fexpr);
    }
    else
    {
        auto texpr = BDDToFormulaWithPatterns(tchild);
        auto fexpr = BDDToFormulaWithPatterns(fchild);
        result = MergeDefault(var, texpr, fexpr);
    }

    assert((!isOr(result) && !isAnd(result)) || result.num_args() == 2);
    expr_cache.emplace(node, result);
    return result;
}

z3::expr SimplifierThread::BDDToFormula(const BDD &bdd)
{
    idx_to_var.clear();
    for (const auto &[name, bvec] : transformer->vars)
    {
        for (int i = 0; i < bvec.bitnum(); ++i)
        {
            int idx = bvec[i].GetBDD().NodeReadIndex();
            idx_to_var[idx] = std::make_pair(name, i);
        }
    }

    expr_cache.clear();
    expr_cache.emplace(BDDNode{Cudd_ReadOne(transformer->bddManager.getManager()), false}, expr.ctx().bool_val(true));
    expr_cache.emplace(BDDNode{Cudd_ReadOne(transformer->bddManager.getManager()), true}, expr.ctx().bool_val(false));
    expr_cache.emplace(BDDNode{Cudd_ReadZero(transformer->bddManager.getManager()), false}, expr.ctx().bool_val(false));
    expr_cache.emplace(BDDNode{Cudd_ReadZero(transformer->bddManager.getManager()), true}, expr.ctx().bool_val(true));

    z3::expr ne(expr.ctx());
    if (settings.bddtof_pattern)
        ne = BDDToFormulaWithPatterns({bdd.getRegularNode(), Cudd_IsComplement(bdd.getNode())});
    else
        ne = BDDToFormula({bdd.getRegularNode(), Cudd_IsComplement(bdd.getNode())});

    if (Solver::resultComputed)
        return expr.ctx().bool_val(false);

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
            return res;
        }
        return e;
    }

    if (e.is_const() && !e.is_numeral())
    {
        vars.emplace(e.to_string(), e);
        return e;
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
    for (auto &[n, v] : vars)
    {
        auto sort = v.get_sort();
        if (!sort.is_bv())
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

VarRange SimplifierThread::GetNodeVar(DdNode *node)
{
    int idx = Cudd_NodeReadIndex(node);
    if (idx_to_var.find(idx) == idx_to_var.end())
        return VarRange(expr.ctx());
    const auto &[name, bit] = idx_to_var.at(idx);
    z3::expr var = vars.at(name);
    return VarRange(var, bit, bit);
}
