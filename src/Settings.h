#pragma once

struct Settings
{
    bool use_over = true;
    bool use_under = false;
    int max_quants = 0;
};

extern Settings settings;