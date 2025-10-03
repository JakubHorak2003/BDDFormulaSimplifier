#pragma once
#include <array>

struct Settings
{
    bool use_over = true;
    bool use_under = false;
    bool use_whole_over = true;
    bool use_whole_under = false;
    int n_approx_pick = 2;
    bool bddtof_pattern = true;
    bool dump_bdds = false;
    bool simplify_whole_formula = false;
    bool replace_precise = true;
    bool show_stats = false;
    std::string output_file = "out.smt2";
};

struct Stats
{
    static constexpr const char *stat_names[] = {
        "subformulas.count",
        "subformulas.precise",
        "subformulas.precise.nontrivial",
        "subformulas.const",
        "subformulas.toplevel.precise",
        "subformulas.toplevel.precise.nontrivial",
        "subformulas.toplevel.const",
        "nodes.count",
        "nodes.merge.eqnum",
        "nodes.merge.eqvar",
        "nodes.merge.ineq",
        "nodes.base.eqvar",
        "nodes.base.ineq"};

    enum Stat
    {
        SUBF_CNT,
        SUBF_PREC,
        SUBF_PREC_NONTRIV,
        SUBF_CONST,
        SUBF_TOP_PREC,
        SUBF_TOP_PREC_NONTRIV,
        SUBF_TOP_CONST,
        NODES_CNT,
        NODES_MERGE_EQNUM,
        NODES_MERGE_EQVAR,
        NODES_MERGE_INEQ,
        NODES_BASE_EQVAR,
        NODES_BASE_INEQ,
        N
    };

    std::array<long long, N> stats{};

    Stats &operator+=(const Stats &other);

    void Dump(std::ostream &file);
};

extern Settings settings;
extern Stats stats;