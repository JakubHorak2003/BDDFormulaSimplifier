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
    tools = list(dct.keys())

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
    # load('results_v5.txt', ['old_q3b', ''] + ['old_'+t for t in SECONDARY_TOOLS], data)
    # load('results_v5_lim_thr.txt', ['', '', '2_new_myapp_over(2)+bitw'], data)
    # load('results_v5_lim_thr_rerun.txt', ['', '2_new_myapp_over(2)+bitw'], data)

    # load('results_v5.txt', ['q3b', ''] + SECONDARY_TOOLS, data)
    # load('results_v5_lim_thr.txt', ['', '', 'myapp+bitw'], data)
    # load('results.txt', ['', 'myapp+bitw'], data)
    
    load('results_v6.txt', ['', 'z3_16', 'q3b_16', 'cvc5_16', 'bitw_16'] + ['' for t in SECONDARY_TOOLS] + ['' for t in SECONDARY_TOOLS] + ['myapp_8+' + t + '_8' for t in SECONDARY_TOOLS], data)
    load('results_v6_10_50.txt', ['', 'z3_60', 'q3b_60', 'cvc5_60', 'bitw_60'], data)
    # load('results_v6_20_40.txt', ['', '', 'z3', 'q3b', 'cvc5', 'bitw'] + ['myapp+' + t for t in SECONDARY_TOOLS], data)
    load('results_v7_20_40.txt', ['', ''] + [f'myapp_20+{t}_40' for t in SECONDARY_TOOLS], data)
    load('results_v7_30_30.txt', ['', ''] + [f'myapp_30+{t}_30' for t in SECONDARY_TOOLS], data)
    load('results.txt', ['', ''] + [f'myapp_10+{t}_50' for t in SECONDARY_TOOLS], data)
    #load('results.txt', ['', ''] + ['30myapp+' + t for t in SECONDARY_TOOLS], data)

    with open('out.csv', 'w') as file:
        keys = list(next(iter(data.values())).keys())
        print(','.join(keys), file=file)
        for bench, res in data.items():
            print(bench,*res.values(),file=file,sep=',')

    total_perf = defaultdict(int)
    sat_perf = defaultdict(int)
    unsat_perf = defaultdict(int)

    myapp_only = 0

    rerun = []
    diff = 0

    myapp_stats = defaultdict(lambda: [0] * 8)

    for benchmark, results_dct in data.items():
        augment_tools(results_dct)

        all_res = set(results_dct.values())
        if {'sat', 'unsat'} <= all_res:
            print('Conflict found', benchmark)
            continue
        
        bench_res = 'sat' if 'sat' in all_res else 'unsat' if 'unsat' in all_res else 'unknown'
        solved_tools = [t for t, r in results_dct.items() if r == bench_res]
        if solved_tools:
            solved_tools.append('total solved')
        by_res_perf = sat_perf if bench_res == 'sat' else unsat_perf if bench_res == 'unsat' else defaultdict(int)
        for t in solved_tools:
            total_perf[t] += 1
            by_res_perf[t] += 1
        total_perf['total'] += 1
        by_res_perf['total'] += 1

        if results_dct['myapp_20+bitw_40'] != results_dct['myapp_10+bitw_50']:
            diff += 1

        if solved_tools and all('myapp' in t or t == 'total solved' for t in solved_tools):
            print('MYAPP', benchmark, bench_res, solved_tools)
            myapp_only += 1

        for st in SECONDARY_TOOLS:
            a = int('q3b_60' in solved_tools)
            b = int(f'{st}_60' in solved_tools)
            c = int(f'myapp_20+{st}_40' in solved_tools)
            myapp_stats[st][a | (b << 1) | (c << 2)] += 1
            if solved_tools:
                myapp_stats[st+'_'+bench_res][a | (b << 1) | (c << 2)] += 1


    print('Total:')
    print_stats(total_perf)

    print('Sat:')
    print_stats(sat_perf)

    print('Unsat:')
    print_stats(unsat_perf)

    print('My app only:', myapp_only)
    print('Diff:', diff)

    print(myapp_stats)

    with open('rerun.txt', 'w') as file:
        file.write('\n'.join(rerun))

if __name__ == "__main__":
    main()
