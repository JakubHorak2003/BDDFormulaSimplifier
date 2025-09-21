#pragma once

struct Settings
{
    bool use_over = true;
    bool use_under = false;
    int max_quants = 0;
    bool bddtof_pattern = true;
    bool dump_bdds = false;
    bool simplify_whole_formula = false;
};

extern Settings settings;