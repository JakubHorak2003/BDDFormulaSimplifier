
if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <benchmark_folder> <timeout>"
    exit 1
fi

BENCHMARK_FOLDER="$1"
TIMEOUT_VAL="$2"
RESULTS_FILE="results.txt"

MYAPP_CMD="../build/myapp"
Z3_CMD="z3"
Q3B_CMD="../build/external/q3b/q3b"
CVC5_CMD="/home/kouba/cvc5/cvc5/build/bin/cvc5"
BITW_CMD="bitwuzla"

> "$RESULTS_FILE"

run_tool() {
    local timeout_val="$1"
    shift
    local output
    output=$(timeout "$timeout_val" "$@" 2>&1)
    local exit_code=$?
    if [ $exit_code -eq 124 ]; then
        echo "timeout"
    elif [ $exit_code -gt 0 ]; then
        echo "crash"
    else
        echo "$output" | tail -n 1
    fi
}

evaluate_mytool() {
    local file="$1"
    local total_timeout="$2"
    local half_timeout=$(( total_timeout / 2 ))

    rm out.smt2 out_o.smt2 out_u.smt2
    cp "$file" out.smt2
    cp "$file" out_o.smt2
    cp "$file" out_u.smt2
    local output
    output=$(timeout "$half_timeout" "$MYAPP_CMD" --verbose:1 --timeout:$((half_timeout - 1)) "$file" 2>&1)
    local exit_code=$?
    echo "$file" >> log.txt
    echo "$output" >> log.txt
    if [ $exit_code -eq 124 ]; then
        echo "myapp_timeout"
    elif [ $exit_code -gt 0 ]; then
        echo "myapp_crash"
    else
        echo "succ"
    fi
}


cat "$BENCHMARK_FOLDER" | while read -r FILE; do
    echo "Processing benchmark: $FILE"

    Z3_RESULT=$(run_tool "$TIMEOUT_VAL" "$Z3_CMD" "$FILE")
    Q3B_RESULT=$(run_tool "$TIMEOUT_VAL" "$Q3B_CMD" "$FILE")
    CVC5_RESULT=$(run_tool "$TIMEOUT_VAL" "$CVC5_CMD" "$FILE")
    BITW_RESULT=$(run_tool "$TIMEOUT_VAL" "$BITW_CMD" "$FILE")

    MY_TOOL_RESULT=$(evaluate_mytool "$FILE" "$TIMEOUT_VAL")

    MYAPP_Z3_RESULT=$(run_tool "$TIMEOUT_VAL" "$Z3_CMD" "out.smt2")
    MYAPP_CVC5_RESULT=$(run_tool "$TIMEOUT_VAL" "$CVC5_CMD" "out.smt2")
    MYAPP_BITW_RESULT=$(run_tool "$TIMEOUT_VAL" "$BITW_CMD" "out.smt2")

    MYAPP_O_Z3_RESULT=$(run_tool "$TIMEOUT_VAL" "$Z3_CMD" "out_o.smt2")
    MYAPP_O_CVC5_RESULT=$(run_tool "$TIMEOUT_VAL" "$CVC5_CMD" "out_o.smt2")
    MYAPP_O_BITW_RESULT=$(run_tool "$TIMEOUT_VAL" "$BITW_CMD" "out_o.smt2")

    MYAPP_U_Z3_RESULT=$(run_tool "$TIMEOUT_VAL" "$Z3_CMD" "out_u.smt2")
    MYAPP_U_CVC5_RESULT=$(run_tool "$TIMEOUT_VAL" "$CVC5_CMD" "out_u.smt2")
    MYAPP_U_BITW_RESULT=$(run_tool "$TIMEOUT_VAL" "$BITW_CMD" "out_u.smt2")

    echo "$FILE, $Q3B_RESULT, $MY_TOOL_RESULT, $Z3_RESULT, $CVC5_RESULT, $BITW_RESULT, $MYAPP_Z3_RESULT, $MYAPP_CVC5_RESULT, $MYAPP_BITW_RESULT, $MYAPP_O_Z3_RESULT, $MYAPP_O_CVC5_RESULT, $MYAPP_O_BITW_RESULT, $MYAPP_U_Z3_RESULT, $MYAPP_U_CVC5_RESULT, $MYAPP_U_BITW_RESULT" >> "$RESULTS_FILE"
done
