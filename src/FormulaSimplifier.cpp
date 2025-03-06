#include <iostream>

#include "FormulaSimplifier.h"

#include "Config.h"
#include "ExprToBDDTransformer.h"
#include "ExprSimplifier.h"

bool isTrue(z3::expr e)
{
    return e.is_app() && e.is_const() && e.decl().decl_kind() == Z3_OP_TRUE;
}
bool isFalse(z3::expr e)
{
    return e.is_app() && e.is_const() && e.decl().decl_kind() == Z3_OP_FALSE;
}

z3::expr simplifyNot(z3::expr e)
{
    if (isTrue(e))
        return e.ctx().bool_val(false);
    if (isFalse(e))
        return e.ctx().bool_val(true);
    return !e;
}

z3::expr simplifyOr(z3::context& ctx, const std::vector<z3::expr>& in)
{
    z3::expr_vector sub(ctx);
    for (const auto& e : in)
    {
        if (isTrue(e))
            return e.ctx().bool_val(true);
        if (isFalse(e))
            continue;
        if (e.is_app() && e.decl().decl_kind() == Z3_OP_OR)
            for (unsigned i = 0; i < e.num_args(); ++i)
                sub.push_back(e.arg(i));
        else if (e.is_app() && e.decl().decl_kind() == Z3_OP_NOT && e.arg(0).is_app() && e.arg(0).decl().decl_kind() == Z3_OP_AND)
            for (unsigned i = 0; i < e.arg(0).num_args(); ++i)
                sub.push_back(!e.arg(0).arg(i));
        else
            sub.push_back(e);
    }
    if (sub.size() == 0)
        return ctx.bool_val(false);
    if (sub.size() == 1)
        return sub[0];
    return z3::mk_or(sub);
}

z3::expr simplifyAnd(z3::context& ctx, const std::vector<z3::expr>& in)
{
    z3::expr_vector sub(ctx);
    for (const auto& e : in)
    {
        if (isFalse(e))
            return e.ctx().bool_val(false);
        if (isTrue(e))
            continue;
        if (e.is_app() && e.decl().decl_kind() == Z3_OP_AND)
            for (unsigned i = 0; i < e.num_args(); ++i)
                sub.push_back(e.arg(i));
        else if (e.is_app() && e.decl().decl_kind() == Z3_OP_NOT && e.arg(0).is_app() && e.arg(0).decl().decl_kind() == Z3_OP_OR)
            for (unsigned i = 0; i < e.arg(0).num_args(); ++i)
                sub.push_back(!e.arg(0).arg(i));
        else
            sub.push_back(e);
    }
    if (sub.size() == 0)
        return ctx.bool_val(true);
    if (sub.size() == 1)
        return sub[0];
    return z3::mk_and(sub);
}

z3::expr simplifyIte(z3::expr c, z3::expr t, z3::expr f)
{
    if (isTrue(c))
        return t;
    if (isFalse(c))
        return f;
    if ((isTrue(t) && isTrue(f)) || (isFalse(t) && isFalse(f)))
        return t;
    if (isTrue(t) && isFalse(f))
        return c;
    if (isFalse(t) && isTrue(f))
        return !c;
    if (isTrue(t))
        return simplifyOr(c.ctx(), {c, f});
    if (isFalse(t))
        return simplifyAnd(c.ctx(), {!c, f});
    if (isTrue(f))
        return simplifyOr(c.ctx(), {!c, t});
    if (isFalse(f))
        return simplifyAnd(c.ctx(), {c, t});
    return z3::ite(c, t, f);
}

z3::expr decorateFormula(z3::expr e, z3::expr under, z3::expr over)
{
    return simplifyAnd(e.ctx(), {simplifyOr(e.ctx(), {e, under}), over}); 
}

z3::expr FormulaSimplifier::Run()
{
    ExprSimplifier simplifier(expr.ctx(), true, true);
    std::cout << "Simplifying...\n";
    expr = simplifier.Simplify(expr);
    CollectVars(expr);
    std::cout << "Vars: " << vars.size() << "\n";
    dumpFormula("simplified.smt2", expr);
    return Simplify(expr);
}

z3::expr FormulaSimplifier::Simplify(z3::expr e)
{
    if (e.is_const())
    {
        return e;
    }

    ExprToBDDTransformer tr(e.ctx(), e, Config());
    std::cout << "Running expr to bdd (over)...\n";
    auto bdds = tr.ProcessOverapproximation(1, 1);

    if (bdds.upper.IsZero())
    {
        std::cout << "Bdd always false\n";
        return e.ctx().bool_val(false);
    }

    if (e.is_app())
    {
        z3::func_decl f = e.decl();
        unsigned num = e.num_args();

        auto decl_kind = f.decl_kind();

        if (decl_kind == Z3_OP_NOT)
        {
            auto sim = Simplify(e.arg(0));
            e = simplifyNot(sim);
        }
        else if (decl_kind == Z3_OP_OR)
        {
            std::vector<z3::expr> sim;
            for (unsigned i = 0; i < num; ++i)
                sim.push_back(Simplify(e.arg(i)));
            e = simplifyOr(e.ctx(), sim);
        }
        else if (decl_kind == Z3_OP_AND)
        {
            std::vector<z3::expr> sim;
            for (unsigned i = 0; i < num; ++i)
                sim.push_back(Simplify(e.arg(i)));
            e = simplifyAnd(e.ctx(), sim);
        }
        else if (decl_kind == Z3_OP_ITE)
        {
            auto c = Simplify(e.arg(0));
            auto t = Simplify(e.arg(1));
            auto f = Simplify(e.arg(2));
            e = simplifyIte(c, t, f);
        }
    }

    if (!bdds.upper.IsOne() && e.is_quantifier())
    {
        std::cout << "Useful result returned\n";
        dumpFormula("f" + std::to_string(next_dot_id) + ".smt2", e);
        return simplifyAnd(e.ctx(), {BDDToFormula(tr, bdds.upper), e});
    }

    return e;
}

z3::expr FormulaSimplifier::BDDToFormula(DdNode* node)
{
    if (expr_cache.find(node) != expr_cache.end())
        return expr_cache.at(node);

    z3::expr texpr = BDDToFormula(Cudd_Regular(Cudd_T(node)));
    z3::expr fexpr = BDDToFormula(Cudd_Regular(Cudd_E(node)));

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

z3::expr FormulaSimplifier::BDDToFormula(ExprToBDDTransformer& tr, const BDD& bdd)
{
    std::cout << "Running bdd to expr...\n";
    dumpBddToDot(bdd);

    expr_cache.clear();
    idx_to_var.clear();
    for (const auto&[name, bvec] : tr.vars)
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

    if (Cudd_IsComplement(bdd.getNode()))
        ne = simplifyNot(ne);

    return ne;
}

void FormulaSimplifier::CollectVars(z3::expr e)
{
    if (e.is_const() && e.decl().decl_kind() != Z3_OP_TRUE && e.decl().decl_kind() != Z3_OP_FALSE)
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

void FormulaSimplifier::dumpBddToDot(const BDD& bdd)
{
    auto filename = "bdd" + std::to_string(next_dot_id) + ".dot";
    auto file = fopen(filename.c_str(), "w");
    next_dot_id++;
    DdNode* nodes[] = {bdd.getNode()};
    Cudd_DumpDot(bdd.manager(), 1, nodes, nullptr, nullptr, file);
    fclose(file);
}

void dumpFormula(const std::string& filename, z3::expr formula)
{
    Z3_set_ast_print_mode(formula.ctx(), Z3_PRINT_SMTLIB2_COMPLIANT);  
    std::string smt2_repr = Z3_benchmark_to_smtlib_string(formula.ctx(), "", "QF_BV", "unknown", "", 0, NULL, formula);

    std::ofstream of(filename);
    of << smt2_repr << std::endl;
}
