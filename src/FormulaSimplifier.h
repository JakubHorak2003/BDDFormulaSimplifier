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

    z3::expr Simplify(z3::expr e, std::list<SimplifierThread>::iterator& t_curr);

    void LaunchThreads(z3::expr e);

private:
    z3::expr expr;

    std::list<SimplifierThread> threads;
};