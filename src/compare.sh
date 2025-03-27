#!/bin/bash
# Usage: ./compare_tools.sh <benchmark_folder> <timeout>
# This script recursively finds all .smt2 files in the given folder and,
# for each file, runs three tests:
#   1. Run z3 on the benchmark with the given timeout.
#   2. Run q3b (located in ./external/q3b/q3b) on the benchmark with the given timeout.
#   3. Run your custom tool by first running ./myapp on the benchmark with half the timeout,
#      then running z3 on the produced out.smt2 with the remaining half timeout.
#
# For each tool, the last line of output is used as its result (sat, unsat or unknown).
# If a tool times out (exit code 124), the result is recorded as "timeout".
# If a tool crashes (exit code > 128), the result is recorded as "crash".
# (In the third tool, if either part fails with timeout or crash, that is recorded.)
#
# A summary line (benchmark name and the 3 results) is appended to results.txt.

# Check that exactly 2 arguments are provided
if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <benchmark_folder> <timeout>"
    exit 1
fi

BENCHMARK_FOLDER="$1"
TIMEOUT_VAL="$2"
RESULTS_FILE="results.txt"

# Empty (or create) the results file
> "$RESULTS_FILE"

# Recursively find all .smt2 files in the given folder
find "$BENCHMARK_FOLDER" -type f -name "*.smt2" | while read -r FILE; do
    echo "Processing benchmark: $FILE"

    # Run z3 on the benchmark with the given timeout
    Z3_OUTPUT=$(timeout "$TIMEOUT_VAL" z3 "$FILE" 2>&1)
    Z3_EXIT=$?
    if [ $Z3_EXIT -eq 124 ]; then
        Z3_RESULT="timeout"
    elif [ $Z3_EXIT -gt 0 ]; then
        Z3_RESULT="crash"
    else
        Z3_RESULT=$(echo "$Z3_OUTPUT" | tail -n 1)
    fi

    # Run q3b on the benchmark with the given timeout
    Q3B_OUTPUT=$(timeout "$TIMEOUT_VAL" ../build/external/q3b/q3b "$FILE" 2>&1)
    Q3B_EXIT=$?
    if [ $Q3B_EXIT -eq 124 ]; then
        Q3B_RESULT="timeout"
    elif [ $Q3B_EXIT -gt 0 ]; then
        Q3B_RESULT="crash"
    else
        Q3B_RESULT=$(echo "$Q3B_OUTPUT" | tail -n 1)
    fi

    # Compute half of the given timeout (integer division)
    HALF_TIMEOUT=$(($TIMEOUT_VAL / 2))

    # Run your custom tool in two parts:
    # Part 1: run ./myapp on the benchmark with half the timeout (preprocessing)
    rm out.smt2
    timeout "$HALF_TIMEOUT" ../build/myapp "$FILE"
    MYAPP_PART1_EXIT=$?
    if [ $MYAPP_PART1_EXIT -gt 0 ] && [ $MYAPP_PART1_EXIT -neq 124 ]; then
        MYAPP_RESULT="crash"
    else
        # Part 2: run z3 on the file produced by your tool (out.smt2) with the other half timeout
        MYAPP_OUTPUT=$(timeout "$HALF_TIMEOUT" z3 out.smt2 2>&1)
        MYAPP_PART2_EXIT=$?
        if [ $MYAPP_PART2_EXIT -eq 124 ]; then
            MYAPP_RESULT="timeout"
        elif [ $MYAPP_PART2_EXIT -gt 0 ]; then
            MYAPP_RESULT="crash"
        else
            MYAPP_RESULT=$(echo "$MYAPP_OUTPUT" | tail -n 1)
        fi
    fi

    # Append a line to the results file: benchmark, z3_result, q3b_result, my_tool_result
    echo "$FILE, $Z3_RESULT, $Q3B_RESULT, $MYAPP_RESULT" >> "$RESULTS_FILE"
    echo "$Z3_RESULT, $Q3B_RESULT, $MYAPP_RESULT"
done
