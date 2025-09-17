#pragma once
#include <z3++.h>

bool isTrue(z3::expr e);
bool isFalse(z3::expr e);

bool isOr(z3::expr e);
bool isAnd(z3::expr e);
bool isIte(z3::expr e);
bool isNot(z3::expr e);

bool isSame(z3::expr a, z3::expr b);

z3::expr simplifyNot(z3::expr e);
z3::expr simplifyOr(z3::context &ctx, const std::vector<z3::expr> &in);
z3::expr simplifyOr(z3::expr a, z3::expr b);
z3::expr simplifyAnd(z3::context &ctx, const std::vector<z3::expr> &in);
z3::expr simplifyAnd(z3::expr a, z3::expr b);
z3::expr simplifyIte(z3::expr c, z3::expr t, z3::expr f);
z3::expr simplifyIte(z3::expr c, z3::expr t, z3::expr f, z3::expr notc);
z3::expr decorateFormula(z3::expr e, z3::expr under, z3::expr over);
