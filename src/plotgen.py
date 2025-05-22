import matplotlib.pyplot as plt
from analyze import load_v2, SECONDARY_TOOLS

def get_data(data, durs, tool):
    x = []
    for b, d in data.items():
        if d[tool] in ['sat', 'unsat']:
            tm = durs[b][tool]
            if tool.startswith('myapp'):
                tm += durs[b]['myapp']
            x.append(min(60, tm / 1000))
    x.sort()
    y = list(range(1, len(x) + 1))
    return x, y


def main():
    data = {}
    durs = {}
    load_v2('results_v8_20_40_complete.txt', ['myapp', 'z3_sg', 'q3b_sg', 'cvc5_sg', 'bitw_sg'] + [f'myapp_sg_20+{t}_40' for t in SECONDARY_TOOLS], data, durs)

    to_pop = []
    for b in data.keys():
        if all((t < 1000 and ('myapp' not in n or t < 500)) for n, t in durs[b].items()):
            to_pop.append(b)
    print(len(to_pop))
    for b in to_pop:
        data.pop(b)
        durs.pop(b)

    plt.figure(figsize=(8, 5))
    for t, l in zip(['q3b_sg', 'bitw_sg', 'myapp_sg_20+bitw_40'], ['Q3B', 'Bitwuzla', 'FBS(20)+Bitwuzla']):
        plt.step(*get_data(data, durs, t), where='post', label=l)
    plt.xlabel('Time (s)')
    plt.ylabel('Non-trivial benchmarks solved')
    plt.legend()
    plt.grid(True)
    plt.tight_layout()
    plt.savefig('plt.pdf')

if __name__ == '__main__':
    main()