#include "ExprManip.h"

bool GetVarRange(z3::expr e, VarRange &res)
{
    if (!e.is_bv())
        return false;
    if (e.is_const() && !e.is_numeral())
    {
        res.var = e;
        res.from = 0;
        res.to = e.get_sort().bv_size() - 1;
        return true;
    }
    else if (e.is_app() && e.decl().decl_kind() == Z3_OP_EXTRACT && e.arg(0).is_const() && !e.arg(0).is_numeral())
    {
        res.var = e.arg(0);
        res.from = Z3_get_decl_int_parameter(e.ctx(), e.decl(), 1);
        res.to = Z3_get_decl_int_parameter(e.ctx(), e.decl(), 0);
        return true;
    }
    return false;
}

bool GetEqNumeral(z3::expr e, EqNumeral &res)
{
    if (!e.is_app() || e.decl().decl_kind() != Z3_OP_EQ || e.num_args() != 2)
        return false;
    if (!GetVarRange(e.arg(0), res.var))
        return false;
    if (!e.arg(1).is_numeral() || !e.arg(1).is_bv())
        return false;
    res.numeral = e.arg(1);
    return true;
}

bool GetEqNumeralInvSB(z3::expr e, EqNumeral &res)
{
    if (!e.is_app() || e.decl().decl_kind() != Z3_OP_EQ || e.num_args() != 2)
        return false;
    if (!GetVarRange(e.arg(0), res.var) || res.var.bv_size() != 1)
        return false;
    if (!e.arg(1).is_numeral() || !e.arg(1).is_bv() || e.arg(1).get_sort().bv_size() != 1)
        return false;
    res.numeral = e.arg(1);
    res.numeral.val = res.numeral.val == "1" ? "0" : "1";
    return true;
}

bool GetEqNumeralInvNeg(z3::expr e, EqNumeral &res)
{
    if (!e.is_app() || e.decl().decl_kind() != Z3_OP_NOT || e.num_args() != 1)
        return false;
    return GetEqNumeral(e.arg(0), res);
}

bool GetEqNumeralInv(z3::expr e, EqNumeral &res)
{
    return GetEqNumeralInvSB(e, res) || GetEqNumeralInvNeg(e, res);
}

bool GetEqVar(z3::expr e, EqVar &res)
{
    if (!e.is_app() || e.decl().decl_kind() != Z3_OP_EQ || e.num_args() != 2)
        return false;
    if (!GetVarRange(e.arg(0), res.a))
        return false;
    if (!GetVarRange(e.arg(1), res.b))
        return false;
    return true;
}

bool GetIneqVar(z3::expr e, IneqVar &res)
{
    if (!e.is_app() || e.num_args() != 2)
        return false;
    int lt = 0;
    if (e.decl().decl_kind() == Z3_OP_SLEQ)
        res.SetIneq(true, true);
    else if (e.decl().decl_kind() == Z3_OP_SLT)
        res.SetIneq(false, true);
    else if (e.decl().decl_kind() == Z3_OP_SGEQ)
        res.SetIneq(true, true), lt = 1;
    else if (e.decl().decl_kind() == Z3_OP_SGT)
        res.SetIneq(false, true), lt = 1;
    else if (e.decl().decl_kind() == Z3_OP_ULEQ)
        res.SetIneq(true, false);
    else if (e.decl().decl_kind() == Z3_OP_ULT)
        res.SetIneq(false, false);
    else if (e.decl().decl_kind() == Z3_OP_UGEQ)
        res.SetIneq(true, false), lt = 1;
    else if (e.decl().decl_kind() == Z3_OP_UGT)
        res.SetIneq(false, false), lt = 1;
    else
        return false;
    if (!GetVarRange(e.arg(lt), res.a))
        return false;
    if (!GetVarRange(e.arg(1 - lt), res.b))
        return false;
    return true;
}
