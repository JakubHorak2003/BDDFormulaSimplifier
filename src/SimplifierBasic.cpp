#include "SimplifierBasic.h"

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
    if (isTrue(under))
        return under;
    return simplifyAnd(e.ctx(), {over, simplifyOr(e.ctx(), {under, e})}); 
}
