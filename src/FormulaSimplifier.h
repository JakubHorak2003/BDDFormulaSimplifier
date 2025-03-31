#pragma once
#include <map>
#include <list>
#include <z3++.h>
#include "ExprToBDDTransformer.h"
#include "SimplifierThread.h"

class FormulaSimplifier
{
public:
    FormulaSimplifier(z3::expr expr) : expr(expr) {}

    z3::expr Run();

    z3::expr Simplify(z3::expr e, std::list<SimplifierThread>::iterator& t_curr, bool use_over, bool use_under);

    void LaunchThreads(z3::expr e, std::vector<z3::expr>& bound);

private:
    z3::expr RemoveInternal(z3::expr e);

    std::vector<z3::expr> PickResults(const std::vector<z3::expr>& approx);
    z3::expr expr;

    std::list<SimplifierThread> threads;
};