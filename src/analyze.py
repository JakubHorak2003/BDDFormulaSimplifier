#!/usr/bin/env python3

import sys
from collections import defaultdict

def is_solved(result):
    return result.lower() in {"sat", "unsat"}

def main():
    if len(sys.argv) < 2:
        filename = "results.txt"
    else:
        filename = sys.argv[1]
    
    total_benchmarks = 0
    solved_by_all = 0
    solved_by_none = 0
    solved_by_one_counts = defaultdict(int)
    conflicts = 0
    conflicts_benchmarks = []

    tool_names = ["z3", "q3b", "cvc5", "myapp then z3", "myapp then cvc5"]
    tool_solved_counts = defaultdict(int)
    
    try:
        with open(filename, "r") as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue 

                parts = [part.strip() for part in line.split(",")]
                if len(parts) != len(tool_names) + 1:
                    print(f"Skipping line with unexpected format: {line}")
                    continue

                benchmark, *results = parts
                total_benchmarks += 1

                results = {
                    tool: res for tool, res in zip(tool_names, results)
                }
                solved_tools = {tool: res.lower() for tool, res in results.items() if is_solved(res)}
                
                # Count each tool's solved benchmarks
                for tool in solved_tools:
                    tool_solved_counts[tool] += 1

                count_solved = len(solved_tools)
                if count_solved == 0:
                    solved_by_none += 1
                if count_solved == 3:
                    solved_by_all += 1
                if count_solved == 1:
                    # Only one tool solved it; record which one
                    only_tool = next(iter(solved_tools.keys()))
                    solved_by_one_counts[only_tool] += 1

                # Detect conflicts: if at least two tools solved it but with differing results.
                if count_solved >= 2:
                    # Create a set of solved results (e.g., {"sat"} or {"sat", "unsat"})
                    solved_results = set(solved_tools.values())
                    if len(solved_results) > 1:
                        conflicts += 1
                        conflicts_benchmarks.append(benchmark)
    except FileNotFoundError:
        print(f"Results file '{filename}' not found.")
        sys.exit(1)

    def pct(count):
        return (count / total_benchmarks * 100) if total_benchmarks else 0

    # Print the analysis
    print(f"Total benchmarks analyzed: {total_benchmarks}\n")
    print(f"Benchmarks solved by all tools: {solved_by_all} ({pct(solved_by_all):.2f}%)")
    print(f"Benchmarks not solved by any tool: {solved_by_none} ({pct(solved_by_none):.2f}%)\n")
    print("Benchmarks solved by only one tool:")
    for tool in tool_names:
        count = solved_by_one_counts[tool]
        print(f"  {tool}: {count} ({pct(count):.2f}%)")
    print()
    print(f"Benchmarks with conflicting results (sat vs unsat): {conflicts} ({pct(conflicts):.2f}%)")
    if conflicts_benchmarks:
        print("List of benchmarks with conflicts:")
        for bench in conflicts_benchmarks:
            print(f"  {bench}")
    print("\nIndividual tool performance:")
    for tool in tool_names:
        count = tool_solved_counts[tool]
        print(f"  {tool} solved {count} benchmarks ({pct(count):.2f}%)")

if __name__ == "__main__":
    main()
