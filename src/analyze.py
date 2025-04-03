#!/usr/bin/env python3

import sys
from collections import defaultdict

def is_solved(result):
    return result == 'sat' or result == 'unsat'

SECONDARY_TOOLS = ['z3', 'cvc5', 'bitw']

def print_stats(data):
    total = data['total']
    for tool, cnt in sorted(data.items(), key=lambda x:-x[1]):
        print(f'    {tool}: {cnt} ({cnt/total*100:.2f} %)')

def combine_results(results):
    results = set(results)
    if {'sat', 'unsat'} <= results:
        return 'crash'
    if 'sat' in results:
        return 'sat'
    if 'unsat' in results:
        return 'unsat'
    if 'crash' in results:
        return 'crash'
    return 'timeout'

def add_tool(dct, subtools):
    name = '||'.join(subtools)
    result = combine_results([dct[t] for t in subtools])
    dct[name] = result

def augment_tools(dct):
    add_tool(dct, [t for t in dct if 'myapp' in t])
    add_tool(dct, [t for t in dct if 'myapp' not in t and t != 'q3b'])
    return
    tools = list(dct.keys())
    for tool_over in tools:
        for tool_under in tools:
            if tool_over >= tool_under:
                continue
            
            name = f'{tool_over}||{tool_under}'
            result = combine_results([dct[tool_over], dct[tool_under]])
            dct[name] = result

def load(filename, tool_names, res):
    with open(filename) as file:
        lines = file.read().strip().split('\n')
    for line in lines:
        benchmark, *results = [p.strip() for p in line.split(',')]
        if benchmark not in res:
            res[benchmark] = {}
        for t, r in zip(tool_names, results):
            if t != '':
                res[benchmark][t] = r

def main():
    data = {}
    # load('results_v5_fixaff_over.txt', ['', 'new_myapp_over+z3', 'new_myapp_over+cvc5', 'new_myapp_over+bitw'], data)
    load('results_v5.txt', ['q3b', ''] + SECONDARY_TOOLS + ['myapp+' + t for t in SECONDARY_TOOLS] + ['myapp_over+' + t for t in SECONDARY_TOOLS] + ['myapp_under+' + t for t in SECONDARY_TOOLS], data)
    # load('results_v5_lim_thr.txt', ['', '2_new_myapp_over(1)+bitw', '2_new_myapp_over(2)+bitw'], data)
    # load('results.txt', ['', '2_new_myapp_over(2)+bitw'], data)

    # load('results_v5.txt', ['q3b', ''] + SECONDARY_TOOLS, data)
    # load('results_v5_lim_thr.txt', ['', '', 'myapp+bitw'], data)
    # load('results.txt', ['', 'myapp+bitw'], data)

    total_perf = defaultdict(int)
    sat_perf = defaultdict(int)
    unsat_perf = defaultdict(int)

    myapp_only = 0

    rerun = []

    for benchmark, results_dct in data.items():
        # augment_tools(results_dct)

        all_res = set(results_dct.values())
        if {'sat', 'unsat'} <= all_res:
            print('Conflict found', benchmark)
            continue
        
        bench_res = 'sat' if 'sat' in all_res else 'unsat' if 'unsat' in all_res else 'unknown'
        solved_tools = [t for t, r in results_dct.items() if r == bench_res]
        by_res_perf = sat_perf if bench_res == 'sat' else unsat_perf
        for t in solved_tools:
            total_perf[t] += 1
            by_res_perf[t] += 1
        total_perf['total'] += 1
        by_res_perf['total'] += 1

        if '2_new_myapp_over(2)+bitw' in solved_tools and 'new_myapp_over+bitw' not in solved_tools:
            print('BETTER', benchmark, bench_res)
            rerun.append(benchmark)
        if '2_new_myapp_over(2)+bitw' not in solved_tools and 'new_myapp_over+bitw' in solved_tools:
            print('WORSE', benchmark, bench_res)
            rerun.append(benchmark)

    print('Total:')
    print_stats(total_perf)

    print('Sat:')
    print_stats(sat_perf)

    print('Unsat:')
    print_stats(unsat_perf)

    print('My app only:', myapp_only)

    with open('rerun.txt', 'w') as file:
        file.write('\n'.join(rerun))

if __name__ == "__main__":
    main()
