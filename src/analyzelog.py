from parse import parse
from collections import defaultdict
import sys

tot = defaultdict(int)
n = 0

with open(sys.argv[1]) as file:
    lines = file.read().strip().split('\n')
for line in lines:
    p = parse('{key}={value:d}', line)
    if p:
        if p['key'] == 'subformulas.count':
            n += 1
        tot[p['key']] += p['value']

print(n)
print(tot)