#include "Pattern.h"
#include "SimplifierBasic.h"

z3::expr MergeEq(const EqVar &cv, z3::expr texpr, z3::expr fexpr)
{
    EqVar cu(texpr.ctx());
    if (isFalse(fexpr) && GetEqVar(texpr, cu) && cu.Merge(cv))
        return cu.ToExpr();
    if (isFalse(fexpr) && isAnd(texpr) && GetEqVar(texpr.arg(0), cu) && cu.Merge(cv))
        return simplifyAnd(cu.ToExpr(), texpr.arg(1));
    if (isIte(texpr) && isSame(texpr.arg(2), fexpr) && GetEqVar(texpr.arg(0), cu) && cu.Merge(cv))
        return simplifyIte(cu.ToExpr(), texpr.arg(1), fexpr);
    if (isOr(texpr) && isSame(texpr.arg(1), fexpr) && GetEqVar(texpr.arg(0), cu) && cu.Merge(cv))
        return simplifyOr(cu.ToExpr(), fexpr);
    if (isTrue(fexpr) && isNot(texpr) && GetEqVar(texpr.arg(0), cu) && cu.Merge(cv))
        return !cu.ToExpr();
    if (isTrue(fexpr) && isOr(texpr) && isNot(texpr.arg(0)) && GetEqVar(texpr.arg(0).arg(0), cu) && cu.Merge(cv))
        return simplifyOr(!cu.ToExpr(), texpr.arg(1));
    if (isAnd(texpr) && isSame(texpr.arg(1), fexpr) && isNot(texpr.arg(0)) && GetEqVar(texpr.arg(0).arg(0), cu) && cu.Merge(cv))
        return simplifyAnd(!cu.ToExpr(), fexpr);
    return simplifyIte(cv.ToExpr(), texpr, fexpr);
}

z3::expr RemoveCommon(z3::expr t, z3::expr f, auto &&combine)
{
    if (isAnd(t) && isAnd(f) && isSame(t.arg(1), f.arg(1)))
        return simplifyAnd(combine(t.arg(0), f.arg(0)), t.arg(1));
    if (isOr(t) && isOr(f) && isSame(t.arg(1), f.arg(1)))
        return simplifyOr(combine(t.arg(0), f.arg(0)), t.arg(1));
    return combine(t, f);
}

z3::expr MergeIneq(const IneqVar &iv, z3::expr texpr, z3::expr fexpr)
{
    IneqVar ivt(texpr.ctx()), ivf(texpr.ctx());
    auto Comb = [&iv, &ivt, &ivf](z3::expr t, z3::expr f)
    {
        if (GetIneqVar(t, ivt) && GetIneqVar(f, ivf) && ivt.a == ivf.a && ivt.b == ivf.b && ivt.eq && !ivf.eq && !ivt.sgn && !ivf.sgn && ivt.Merge(iv))
            return ivt.ToExpr();
        if (GetIneqVar(t, ivt) && GetIneqVar(f, ivf) && ivt.a == ivf.a && ivt.b == ivf.b && !ivt.eq && ivf.eq && !ivt.sgn && !ivf.sgn && ivt.Merge(iv.Inverted()))
            return ivt.ToExpr();
        return simplifyIte(iv.ToExpr(), t, f, iv.Inverted().ToExpr());
    };
    if (isIte(texpr) && isIte(fexpr) && isSame(texpr.arg(1), fexpr.arg(2)) && isSame(texpr.arg(2), fexpr.arg(1)) &&
        GetIneqVar(texpr.arg(0), ivt) && GetIneqVar(fexpr.arg(0), ivf) && ivt.a == ivf.b && ivt.b == ivf.a && ivt.eq && ivf.eq && !ivt.sgn && !ivf.sgn && ivt.Merge(iv))
    {
        return simplifyIte(ivt.ToExpr(), texpr.arg(1), texpr.arg(2));
    }
    return RemoveCommon(texpr, fexpr, Comb);
}

z3::expr MergeDefault(const VarRange &var, z3::expr texpr, z3::expr fexpr)
{
    EqNumeral cnt(var, NumeralVal("1"));
    EqNumeral cnf(var, NumeralVal("0"));

    EqNumeral cm(texpr.ctx());
    if (isIte(texpr) && isSame(texpr.arg(2), fexpr) && GetEqNumeral(texpr.arg(0), cm) && cm.Merge(cnt))
        return simplifyIte(cm.ToExpr(), texpr.arg(1), fexpr);
    if (isIte(fexpr) && isSame(fexpr.arg(2), texpr) && GetEqNumeral(fexpr.arg(0), cm) && cm.Merge(cnf))
        return simplifyIte(cm.ToExpr(), fexpr.arg(1), texpr);
    if (isIte(texpr) && isSame(texpr.arg(1), fexpr) && GetEqNumeralInv(texpr.arg(0), cm) && cm.Merge(cnt))
        return simplifyIte(cm.ToExpr(), texpr.arg(2), fexpr);
    if (isIte(fexpr) && isSame(fexpr.arg(1), texpr) && GetEqNumeralInv(fexpr.arg(0), cm) && cm.Merge(cnf))
        return simplifyIte(cm.ToExpr(), fexpr.arg(2), texpr);
    if (isOr(texpr) && isSame(texpr.arg(1), fexpr) && GetEqNumeral(texpr.arg(0), cm) && cm.Merge(cnt))
        return simplifyOr(cm.ToExpr(), fexpr);
    if (isOr(fexpr) && isSame(fexpr.arg(1), texpr) && GetEqNumeral(fexpr.arg(0), cm) && cm.Merge(cnf))
        return simplifyOr(cm.ToExpr(), texpr);
    if (isAnd(texpr) && isSame(texpr.arg(1), fexpr) && GetEqNumeralInv(texpr.arg(0), cm) && cm.Merge(cnt))
        return simplifyAnd(!cm.ToExpr(), fexpr);
    if (isAnd(fexpr) && isSame(fexpr.arg(1), texpr) && GetEqNumeralInv(fexpr.arg(0), cm) && cm.Merge(cnf))
        return simplifyAnd(!cm.ToExpr(), texpr);

    bool use_and = false, use_or = false;
    auto ttemp = simplifyAnd(cnt.ToExpr(), texpr);
    auto ftemp = simplifyAnd(cnf.ToExpr(), fexpr);
    auto ttemp_c = simplifyOr(cnf.ToExpr(), texpr);
    auto ftemp_c = simplifyOr(cnt.ToExpr(), fexpr);

    if (!isTrue(fexpr) && GetEqNumeral(texpr, cm) && cm.Merge(cnt))
        ttemp = cm.ToExpr(), use_or = true;
    else if (!isTrue(fexpr) && isAnd(texpr) && GetEqNumeral(texpr.arg(0), cm) && cm.Merge(cnt))
        ttemp = simplifyAnd(cm.ToExpr(), texpr.arg(1)), use_or = true;
    else if (GetEqNumeralInv(texpr, cm) && cm.Merge(cnt))
        ttemp_c = !cm.ToExpr(), use_and = true;
    else if (isOr(texpr) && GetEqNumeralInv(texpr.arg(0), cm) && cm.Merge(cnt))
        ttemp_c = simplifyOr(!cm.ToExpr(), texpr.arg(1)), use_and = true;

    if (!isTrue(texpr) && GetEqNumeral(fexpr, cm) && cm.Merge(cnf))
        ftemp = cm.ToExpr(), use_or = true;
    else if (!isTrue(texpr) && isAnd(fexpr) && GetEqNumeral(fexpr.arg(0), cm) && cm.Merge(cnf))
        ftemp = simplifyAnd(cm.ToExpr(), fexpr.arg(1)), use_or = true;
    else if (GetEqNumeralInv(fexpr, cm) && cm.Merge(cnf))
        ftemp_c = !cm.ToExpr(), use_and = true;
    else if (isOr(fexpr) && GetEqNumeralInv(fexpr.arg(0), cm) && cm.Merge(cnf))
        ftemp_c = simplifyOr(!cm.ToExpr(), fexpr.arg(1)), use_and = true;

    if (use_or)
        return simplifyOr(ttemp, ftemp);
    if (use_and)
        return simplifyAnd(ttemp_c, ftemp_c);
    return simplifyIte(cnt.ToExpr(), texpr, fexpr, cnf.ToExpr());
}
