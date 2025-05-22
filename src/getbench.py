import os

def list_smt2_files(base_dir, output_file):
    with open(output_file, 'w') as out:
        for root, _, files in os.walk(base_dir):
            for file in files:
                if file.endswith('.smt2'):
                    rel_path = os.path.join(root, file)
                    out.write(rel_path + '\n')

def remove_excluded_files(source_file, exclude_file, result_file):
    with open(source_file, 'r') as src:
        source_lines = set(line.strip() for line in src)

    with open(exclude_file, 'r') as excl:
        exclude_lines = set(line.strip() for line in excl)

    remaining = sorted(source_lines - exclude_lines)

    with open(result_file, 'w') as out:
        for line in remaining:
            out.write(line + '\n')

# Example usage
base_directory = '../bench/non-incremental/BV'
all_files_txt = 'all_files.txt'
exclude_txt = 'bench.txt'
filtered_txt = 'bench_rem.txt'

list_smt2_files(base_directory, all_files_txt)
remove_excluded_files(all_files_txt, exclude_txt, filtered_txt)
