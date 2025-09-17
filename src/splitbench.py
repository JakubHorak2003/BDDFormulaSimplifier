import sys
import random

with open(sys.argv[1]) as file:
    lines = file.read().strip().split('\n')

N = int(sys.argv[2])

random.shuffle(lines)

for i in range(N):
    nl = lines[i::N]
    with open(f'all_files_{i}.txt', 'w') as file:
        for line in nl:
            print(line, file=file)
