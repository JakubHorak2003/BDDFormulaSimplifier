#pragma once
#include <utility>
#include <z3++.h>

#include "Settings.h"

struct VarRange
{
    z3::expr var;
    int from = 0;
    int to = 0;

    VarRange(z3::context &c) : var(c) {}
    VarRange(z3::expr e, int f, int t) : var(e), from(f), to(t) {}

    z3::expr ToExpr() const
    {
        int bl = var.get_sort().bv_size();
        if (from == 0 && to >= bl - 1)
            return var;
        return var.extract(to, from);
    }

    bool operator==(const VarRange &other) const { return var.to_string() == other.var.to_string() && from == other.from && to == other.to; }

    int bv_size() const { return to - from + 1; }

    int IsAdjacent(const VarRange &other)
    {
        if (var.to_string() != other.var.to_string())
            return 0;
        if (other.from == to + 1)
            return 1;
        if (from == other.to + 1)
            return -1;
        return 0;
    }

    bool IsInvalid() const
    {
        return var.to_string() == "null" || !var.is_bv();
    }
};

bool GetVarRange(z3::expr e, VarRange &res);

struct NumeralVal
{
    std::string val;

    NumeralVal() = default;
    NumeralVal(const std::string &v) : val(v) {}
    NumeralVal(z3::expr e)
    {
        val = Z3_get_numeral_binary_string(e.ctx(), e);
        int bl = e.get_sort().bv_size();
        val = std::string(bl - (int)val.size(), '0') + val;
    }

    void Extend(bool msb, const NumeralVal &other)
    {
        if (msb)
            val = other.val + val;
        else
            val += other.val;
    }

    z3::expr ToExpr(z3::context &c) const
    {
        bool *temp = new bool[val.size()];
        for (int i = 0; i < (int)val.size(); ++i)
            temp[i] = val[(int)val.size() - 1 - i] == '1';
        auto res = c.bv_val(val.size(), temp);
        delete[] temp;
        return res;
    }
};

struct EqNumeral
{
    VarRange var;
    NumeralVal numeral;

    EqNumeral(z3::context &c) : var(c) {}
    EqNumeral(const VarRange &vr, const NumeralVal &nv) : var(vr), numeral(nv) {}

    z3::expr ToExpr() const
    {
        return var.ToExpr() == numeral.ToExpr(var.var.ctx());
    }

    bool Merge(const EqNumeral &other)
    {
        int adj = var.IsAdjacent(other.var);
        if (adj == 1)
        {
            var.to = other.var.to;
            numeral.Extend(true, other.numeral);
            ++stats.stats[Stats::NODES_MERGE_EQNUM];
            return true;
        }
        if (adj == -1)
        {
            var.from = other.var.from;
            numeral.Extend(false, other.numeral);
            ++stats.stats[Stats::NODES_MERGE_EQNUM];
            return true;
        }
        return false;
    }
};

struct EqVar
{
    VarRange a;
    VarRange b;

    EqVar(z3::context &c) : a(c), b(c) {}
    EqVar(const VarRange &a, const VarRange &b) : a(a), b(b) {}

    z3::expr ToExpr() const
    {
        return a.ToExpr() == b.ToExpr();
    }

    bool Merge(const EqVar &other)
    {
        int adja = a.IsAdjacent(other.a);
        int adjb = b.IsAdjacent(other.b);
        if (adja == 1 && adjb == 1)
        {
            a.to = other.a.to;
            b.to = other.b.to;
            ++stats.stats[Stats::NODES_MERGE_EQVAR];
            return true;
        }
        if (adja == -1 && adjb == -1)
        {
            a.from = other.a.from;
            b.from = other.b.from;
            ++stats.stats[Stats::NODES_MERGE_EQVAR];
            return true;
        }
        return false;
    }
};

struct IneqVar
{
    VarRange a;
    VarRange b;
    bool eq = false;
    bool sgn = false;

    IneqVar(z3::context &c) : a(c), b(c) {}
    IneqVar(const VarRange &a, const VarRange &b, bool e, bool s = false) : a(a), b(b), eq(e), sgn(s) {}

    z3::expr ToExpr() const
    {
        if (sgn)
        {
            if (eq)
                return a.ToExpr() <= b.ToExpr();
            return a.ToExpr() < b.ToExpr();
        }
        else
        {
            if (eq)
                return z3::ule(a.ToExpr(), b.ToExpr());
            return z3::ult(a.ToExpr(), b.ToExpr());
        }
    }

    IneqVar Inverted() const
    {
        return IneqVar(b, a, !eq, sgn);
    }

    void SetIneq(bool e, bool s)
    {
        eq = e;
        sgn = s;
    }

    bool Merge(const IneqVar &other)
    {
        if (eq != other.eq != sgn != other.sgn)
            return false;
        int adja = a.IsAdjacent(other.a);
        int adjb = b.IsAdjacent(other.b);
        if (adja == 1 && adjb == 1)
        {
            a.to = other.a.to;
            b.to = other.b.to;
            ++stats.stats[Stats::NODES_MERGE_INEQ];
            return true;
        }
        if (adja == -1 && adjb == -1)
        {
            a.from = other.a.from;
            b.from = other.b.from;
            ++stats.stats[Stats::NODES_MERGE_INEQ];
            return true;
        }
        return false;
    }
};

bool GetEqNumeral(z3::expr e, EqNumeral &res);
bool GetEqNumeralInvSB(z3::expr e, EqNumeral &res);
bool GetEqNumeralInvNeg(z3::expr e, EqNumeral &res);
bool GetEqNumeralInv(z3::expr e, EqNumeral &res);
bool GetEqVar(z3::expr e, EqVar &res);
bool GetIneqVar(z3::expr e, IneqVar &res);
