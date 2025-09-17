from analyze import load_v2

data = {}
res = load_v2('results.txt', [], data)

with open('all_files.txt') as file:
    all = file.read().strip().split('\n')

print(len(all), len(data))

rem = set(all) - set(data.keys())

print(len(rem))

with open('rerun.txt', 'w') as file:
    for bnch in rem:
        print(bnch, file=file)
