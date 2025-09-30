lines = []
for i in range(8):
    with open(f'tmp{i}/log.txt') as file:
        lines += file.read().strip().split('\n')

with open('log.txt', 'w') as file:
    file.write('\n'.join(lines))