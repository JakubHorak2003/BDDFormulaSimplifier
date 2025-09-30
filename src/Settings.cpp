#include <string>
#include <fstream>
#include "Settings.h"

Settings settings;
Stats stats;

Stats &Stats::operator+=(const Stats &other)
{
    for (int i = 0; i < N; ++i)
        stats[i] += other.stats[i];
    return *this;
}

void Stats::Dump(std::ostream &file)
{
    for (int i = 0; i < N; ++i)
        file << stat_names[i] << "=" << stats[i] << '\n';
}
