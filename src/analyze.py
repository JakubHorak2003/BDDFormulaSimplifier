#!/usr/bin/env python3

import sys
from collections import defaultdict

def is_solved(result):
    """Return True if the result indicates a solved benchmark (sat or unsat)."""
    return result.lower() in {"sat", "unsat"}

def main():
    # Use the first command-line argument as the results filename, defaulting to "results.txt"
    if len(sys.argv) < 2:
        filename = "results.txt"
    else:
        filename = sys.argv[1]
    
    total_benchmarks = 0
    solved_by_all = 0
    solved_by_none = 0
    solved_by_one_counts = defaultdict(int)  # counts for each tool if only one solved the benchmark
    conflicts = 0
    conflicts_benchmarks = []

    # Track individual tool performance
    tool_names = ["z3", "q3b", "my_tool"]
    tool_solved_counts = defaultdict(int)
    
    try:
        with open(filename, "r") as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue  # skip empty lines

                # Expected format: benchmark, z3_result, q3b_result, my_tool_result
                parts = [part.strip() for part in line.split(",")]
                if len(parts) != 4:
                    print(f"Skipping line with unexpected format: {line}")
                    continue

                benchmark, z3_res, q3b_res, my_tool_res = parts
                total_benchmarks += 1

                # Build a dictionary with tool names and their result (lowercase)
                results = {
                    "z3": z3_res,
                    "q3b": q3b_res,
                    "my_tool": my_tool_res,
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
