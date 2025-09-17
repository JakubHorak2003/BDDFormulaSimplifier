#!/usr/bin/env python3

import sys
from collections import defaultdict

def is_solved(result):
    return result == 'sat' or result == 'unsat'

SECONDARY_TOOLS = ['z3', 'cvc5', 'bitw']

def print_stats(data, rem=0):
    total = data['total'] + rem
    for tool, cnt in sorted(data.items(), key=lambda x:-x[1]):
        cnt += rem
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
    # keys= list(dct.keys())
    # for k1 in keys:
    #     for k2 in keys:
    #         if k1 < k2:
    #             add_tool
    pass
    # add_tool(dct, ['q3b_300', 'bitw_300'])
    # add_tool(dct, ['fbs_pat_100+bitw_200', 'bitw_300'])

    # for t in SECONDARY_TOOLS:
    #     vt = [f'{t}_60'] + [f'myapp_{x}+{t}_{60-x}' for x in [10, 20, 30]]
    #     dct[f'vbs_{t}'] = combine_results([dct[x] for x in vt])
    # for x in [10, 20, 30]:
    #     vt = [f'myapp_{x}+{t}_{60-x}' for t in SECONDARY_TOOLS]
    #     dct[f'vbs_{x}'] = combine_results([dct[t] for t in vt])
    # vt = ['q3b_60'] + [f'{t}_60' for t in SECONDARY_TOOLS]
    # dct[f'vbs_0'] = combine_results([dct[t] for t in vt])
    # dct[f'vbs_1'] = combine_results([v for k, v in dct.items() if 'myapp' in k])

def load(filename, tool_names, res, durs=None):
    with open(filename) as file:
        lines = file.read().strip().split('\n')
    for line in lines:
        benchmark, *results = [p.strip() for p in line.split(',')]
        if benchmark not in res:
            res[benchmark] = {}
        for t, r in zip(tool_names, results):
            if t != '':
                res[benchmark][t] = r

def load_v2(filename, tool_names, res, durs=None):
    with open(filename) as file:
        lines = file.read().strip().split('\n')
    for line in lines:
        benchmark, *results = [p.strip() for p in line.strip().split(',')]
        if benchmark not in res:
            res[benchmark] = {}
        if durs is not None and benchmark not in durs:
            durs[benchmark] = {}
        for t, r, d in zip(tool_names, results[::2], results[1::2]):
            if t != '':
                res[benchmark][t] = r
                if durs is not None:
                    try:
                        durs[benchmark][t] = int(d)
                    except ValueError:
                        print('ERROR', line)

COMPARE = ['fbs_pat_100+bitw_200', 'fbs_100+bitw_200']
# COMPARE = ['fbs_pat_20+bitw_40', 'bitw_300']

def analyze_durs(data, durs, tools):
    sm = {t:0.0 for t in tools}
    tot = 0
    for bench in data.keys():
        if any(t not in data[bench] or data[bench][t] == 'timeout' for t in tools):
            continue
        tot += 1
        for t in tools:
            sm[t] += durs[bench][t]

    for t in tools:
        sm[t] /= tot
    print(tot, sm)

def main():
    data = {}
    durs = {}
    # load('results_v5_fixaff_over.txt', ['', 'new_myapp_over+z3', 'new_myapp_over+cvc5', 'new_myapp_over+bitw'], data)
    # load('results_v5.txt', ['q3b', ''] + SECONDARY_TOOLS + ['myapp_'+t for t in SECONDARY_TOOLS] + ['myappover_'+t for t in SECONDARY_TOOLS] + ['myappunder_'+t for t in SECONDARY_TOOLS], data)
    # load('results_v5_lim_thr.txt', ['', '', '2_new_myapp_over(2)+bitw'], data)
    # load('results_v5_lim_thr_rerun.txt', ['', '2_new_myapp_over(2)+bitw'], data)

    # load('results_v5.txt', ['q3b', ''] + SECONDARY_TOOLS, data)
    # load('results_v5_lim_thr.txt', ['', '', 'myapp+bitw'], data)
    # load('results.txt', ['', 'myapp+bitw'], data)
    
    # load('results_v6.txt', ['', 'z3_v6', 'q3b_v6', 'cvc5_v6', 'bitw_v6'] + ['dump_' + t for t in SECONDARY_TOOLS] + ['simpl_' + t for t in SECONDARY_TOOLS] + ['myapp_' + t + '_v6' for t in SECONDARY_TOOLS], data)
    #load('results_v6_10_50.txt', ['', 'z3_60', 'q3b_60', 'cvc5_60', 'bitw_60'], data)
    # load('results_v6_20_40.txt', ['', '', 'z3', 'q3b', 'cvc5', 'bitw'] + ['myapp+' + t for t in SECONDARY_TOOLS], data)
    #load('results_v7_20_40.txt', ['', ''] + [f'myapp_20+{t}_40' for t in SECONDARY_TOOLS], data)
    #load('results_v7_30_30.txt', ['', ''] + [f'myapp_30+{t}_30' for t in SECONDARY_TOOLS], data)
    #load('results_v7_10_50.txt', ['', ''] + [f'myapp_10+{t}_50' for t in SECONDARY_TOOLS], data)
    # load_v2('results_v8_20_40.txt', ['', 'z3_sg', 'q3b_sg', 'cvc5_sg', 'bitw_sg'] + [f'myapp_sg_20+{t}_40' for t in SECONDARY_TOOLS], data)
    # load_v2('results_60s_sg.txt', ['', 'OLD_z3_sg', 'OLD_q3b_sg', 'OLD_cvc5_sg', 'OLD_bitw_sg'] + [f'OLD_myapp_sg_20+{t}_40' for t in SECONDARY_TOOLS], data)
    # load_v2('results_60s_sg_new.txt', ['', 'z3_60', 'q3b_60', 'cvc5_60', 'bitw_60'] + [f'fbs_20+{t}_40' for t in SECONDARY_TOOLS], data)
    # load_v2('results_60s_sg_pattern.txt', ['', 'PAT_z3_sg', 'PAT_q3b_sg', 'PAT_cvc5_sg', 'PAT_bitw_sg'] + [f'PAT_myapp_sg_20+{t}_40' for t in SECONDARY_TOOLS], data)
    load_v2('results_60s_sg_pat.txt', ['', '', '', '', ''] + [f'fbs_pat_20+{t}_40' for t in SECONDARY_TOOLS], data, durs)
    load_v2('results.txt', ['', 'z3_300', 'q3b_300', 'cvc5_300', 'bitw_300'] + [f'fbs_100+{t}_200' for t in SECONDARY_TOOLS], data, durs)
    load_v2('results_300s_sg_pat.txt', ['', 'z3_300', 'q3b_300', 'cvc5_300', 'bitw_300'] + [f'fbs_pat_100+{t}_200' for t in SECONDARY_TOOLS], data, durs)
    # load_v2('results_300s_sg_pat_20+280.txt', ['', '', '', '', ''] + [f'fbs_pat_20+{t}_280' for t in SECONDARY_TOOLS], data)

    # load('results_v6_10_50.txt', ['', '', 'q3b', '', 'bitw'], data)
    # load('results_v7_20_40.txt', ['', ''] + ['fbs' if t == 'bitw' else '' for t in SECONDARY_TOOLS], data)

    analyze_durs(data, durs, ['bitw_300', 'fbs_pat_100+bitw_200', 'fbs_100+bitw_200'])

    all_tools = list(next(iter(data.values())).keys())
    # for i in range(315):
    #     data[f'sat{i}'] = {t:'sat' for t in all_tools}
    # for i in range(4603):
    #     data[f'unsat{i}'] = {t:'unsat' for t in all_tools}

    with open('out.csv', 'w') as file:
        keys = list(next(iter(data.values())).keys())
        print(','.join(keys), file=file)
        for bench, res in data.items():
            print(bench,*res.values(),file=file,sep=',')

    total_perf = defaultdict(int)
    sat_perf = defaultdict(int)
    unsat_perf = defaultdict(int)

    myapp_only = 0
    better = worse = 0

    rerun = []
    diff = 0

    myapp_stats = defaultdict(lambda: [0] * 8)

    for benchmark, results_dct in data.items():
        augment_tools(results_dct)

        all_res = set(results_dct.values())
        if {'sat', 'unsat'} <= all_res:
            print('Conflict found', benchmark)
            continue

        if 'fbs_100+z3_200' not in results_dct:
            continue
        
        bench_res = 'sat' if 'sat' in all_res else 'unsat' if 'unsat' in all_res else 'unknown'
        solved_tools = [t for t, r in results_dct.items() if r == bench_res]
        if solved_tools:
            solved_tools.append('total solved')

        # a = int('q3b' in solved_tools)
        # b = int(f'bitw' in solved_tools)
        # c = int(f'fbs' in solved_tools)
        # if a and b and c:
        #     solved_tools.append('solved by all')
        # if not a and not b and not c:
        #     solved_tools.append('solved by none')
        # if not a and not b and c:
        #     solved_tools.append('solved by fbs only')
        # if a and b and not c:
        #     solved_tools.append('solved by all but fbs')
        # if c and not b:
        #     solved_tools.append('solved by fbs but not bitwuzla')
        # if not c and b:
        #     solved_tools.append('solved by bitwuzla but not fbs')
        # if c and not a:
        #     solved_tools.append('solved by fbs but not q3b')
        # if not c and a:
        #     solved_tools.append('solved by q3b but not fbs')

        by_res_perf = sat_perf if bench_res == 'sat' else unsat_perf if bench_res == 'unsat' else defaultdict(int)
        for t in solved_tools:
            total_perf[t] += 1
            by_res_perf[t] += 1
        total_perf['total'] += 1
        by_res_perf['total'] += 1

        if COMPARE[1] not in solved_tools and COMPARE[0] in solved_tools:
            print('BETTER', benchmark)
            better += 1
        if COMPARE[1] in solved_tools and COMPARE[0] not in solved_tools:
            print('WORSE', benchmark)
            worse += 1

        if solved_tools and all('fbs' in t or t == 'total solved' for t in solved_tools):
            print('MYAPP', benchmark, bench_res, solved_tools)
            myapp_only += 1



    print('Total:')
    print_stats(total_perf)

    print('Sat:')
    print_stats(sat_perf)

    print('Unsat:')
    print_stats(unsat_perf)

    print('My app only:', myapp_only)
    print('Better:', better)
    print('Worse:', worse)

    print(myapp_stats)

    with open('rerun.txt', 'w') as file:
        file.write('\n'.join(rerun))

if __name__ == "__main__":
    main()
