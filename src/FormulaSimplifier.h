#include <map>
#include <z3++.h>
#include "ExprToBDDTransformer.h"

void dumpFormula(const std::string& filename, z3::expr formula);

class FormulaSimplifier
{
public:
    FormulaSimplifier(z3::expr expr) : expr(expr) {}

    z3::expr Run();

    z3::expr Simplify(z3::expr e);

    z3::expr BDDToFormula(DdNode* node);
    z3::expr BDDToFormula(ExprToBDDTransformer& tr, const BDD& bdd);

    void CollectVars(z3::expr e);

private:
    void dumpBddToDot(const BDD& bdd);

    z3::expr expr;
    int next_dot_id = 0;

    std::map<std::string, z3::expr> vars;
    std::map<const DdNode*, z3::expr> expr_cache;
    std::map<int, std::pair<std::string, int>> idx_to_var;
};