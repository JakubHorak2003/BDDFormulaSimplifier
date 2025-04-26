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
    expr = CollectVars(expr, 0);

    transformer = std::make_unique<ExprToBDDTransformer>(expr.ctx(), expr, Config());

    RunApprox();

    finished = true;
}

void SimplifierThread::RunApprox()
{
    if (Solver::resultComputed)
        return;
        
    if (!overapproximate)
        transformer->setApproximationType(ZERO_EXTEND);

    std::string approx_str = overapproximate ? "over" : "under";

    int bw = 1;
    int prec = 1;
    std::vector<int> node_counts;
    node_counts.push_back(0);
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
        const auto& bdd = overapproximate ? bdds.upper : bdds.lower;
        // logger.DumpFormulaBDD(expr, bdd.upper);

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
            auto cand = FixUnder(expr.ctx().bool_val(true), bw);
            assert(!isFalse(cand));
            result.push_back(cand);
            return;
        }

        int nc = bdd.nodeCount();
        if (nc != node_counts.back() && !bdd.IsZero() && !bdd.IsOne())
        {
            logger.Log("Useful result returned (" + std::to_string(bdd.nodeCount()) + " nodes) " + approx_str + " " + std::to_string(bw) + " " + std::to_string(prec));
            auto cand = BDDToFormula(bdd);
            // auto cand = BDDToFormulaApprox(bdd, 5);
            // logger.DumpFormulaBDD(cand, bdd);
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
            assert(!isFalse(cand) && !isTrue(cand));
            result.push_back(cand);
        }

        if (transformer->OperationApproximationHappened())
            prec *= 4;
        else if (bw == 1)
            bw = 2;
        else
            bw += 2;
    }

    logger.Log("Done");
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

ApproxExpr SimplifierThread::BDDToFormulaApprox(DdNode *node, std::size_t max_size)
{
    if (approx_expr_cache.find(node) != approx_expr_cache.end())
        return approx_expr_cache.at(node);

    const auto& texpr = BDDToFormulaApprox(Cudd_Regular(Cudd_T(node)), max_size);
    if (Solver::resultComputed)
        return ApproxExpr(expr.ctx());
    const auto& fexpr = BDDToFormulaApprox(Cudd_Regular(Cudd_E(node)), max_size);
    if (Solver::resultComputed)
        return ApproxExpr(expr.ctx());

    auto fpo = Cudd_IsComplement(Cudd_E(node)) ? fexpr.pths_zero : fexpr.pths_one;
    auto fpz = Cudd_IsComplement(Cudd_E(node)) ? fexpr.pths_one : fexpr.pths_zero;
    auto tpo = texpr.pths_one;
    auto tpz = texpr.pths_zero;

    int idx = Cudd_NodeReadIndex(node);
    const auto&[name, bit] = idx_to_var.at(idx);
    z3::expr var = vars.at(name).is_bool() ? vars.at(name) : vars.at(name).extract(bit, bit) == expr.ctx().bv_val(1, 1);
    z3::expr neg = vars.at(name).is_bool() ? !vars.at(name) : vars.at(name).extract(bit, bit) == expr.ctx().bv_val(0, 1);

    ApproxExpr result(expr.ctx());
    tpo.AddConstraint(var);
    tpz.AddConstraint(var);
    fpo.AddConstraint(neg);
    fpz.AddConstraint(neg);
    result.pths_one = tpo.MergeWith(fpo, max_size);
    result.pths_zero = tpz.MergeWith(fpz, max_size);
    approx_expr_cache.emplace(node, result);
    return result;
}

z3::expr SimplifierThread::BDDToFormulaApprox(const BDD &bdd, std::size_t max_size)
{
    approx_expr_cache.clear();
    idx_to_var.clear();
    for (const auto&[name, bvec] : transformer->vars)
    {
        for (int i = 0; i < bvec.bitnum(); ++i)
        {
            int idx = bvec[i].GetBDD().NodeReadIndex();
            idx_to_var[idx] = std::make_pair(name, i);
        }
    }
    ApproxExpr false_expr(expr.ctx());
    ApproxExpr true_expr(expr.ctx());
    false_expr.pths_zero.clauses.emplace_back();
    true_expr.pths_one.clauses.emplace_back();
    approx_expr_cache.emplace(Cudd_ReadOne(bdd.manager()), true_expr);
    approx_expr_cache.emplace(Cudd_ReadZero(bdd.manager()), false_expr);

    const auto& ne = BDDToFormulaApprox(bdd.getRegularNode(), max_size);
    if (Solver::resultComputed)
        return expr.ctx().bool_val(false);

    const auto& po = Cudd_IsComplement(bdd.getNode()) ? ne.pths_zero : ne.pths_one;
    const auto& pz = Cudd_IsComplement(bdd.getNode()) ? ne.pths_one : ne.pths_zero;

    if (overapproximate)
        return !pz.ToFormula();
    return po.ToFormula();
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

z3::expr ApproxDNF::ToFormula() const
{
    std::vector<z3::expr> clause_formulas;
    for (const auto& c : clauses)
        clause_formulas.push_back(simplifyAnd(*ctx, c));
    return simplifyOr(*ctx, clause_formulas);
}

void ApproxDNF::AddConstraint(z3::expr e)
{
    for (auto& c : clauses)
        c.push_back(e);
}

ApproxDNF ApproxDNF::MergeWith(const ApproxDNF &other, std::size_t max_size)
{
    assert(ctx == other.ctx);
    ApproxDNF res(*ctx);
    std::merge(clauses.begin(), clauses.end(), other.clauses.begin(), other.clauses.end(), std::back_inserter(res.clauses), [&](const auto& a, const auto& b) { return a.size() < b.size(); });
    if (res.clauses.size() > max_size)
        res.clauses.resize(max_size);
    return res;
}
