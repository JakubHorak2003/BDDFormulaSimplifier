from parse import parse
from collections import defaultdict
import sys

tot = defaultdict(int)

with open(sys.argv[1]) as file:
    lines = file.read().strip().split('\n')
for line in lines:
    p = parse('{key}={value:d}', line)
    if p:
        tot[p['key']] += p['value']

print(tot)