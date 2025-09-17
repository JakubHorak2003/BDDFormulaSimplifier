#pragma once
#include <z3++.h>

#include "ExprManip.h"

z3::expr MergeEq(const EqVar &cv, z3::expr texpr, z3::expr fexpr);
z3::expr MergeIneq(const IneqVar &iv, z3::expr texpr, z3::expr fexpr);
z3::expr MergeDefault(const VarRange &var, z3::expr texpr, z3::expr fexpr);