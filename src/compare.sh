
if [ "$#" -ne 3 ]; then
    echo "Usage: $0 <benchmark_folder> <timeout> <tmo for myapp>"
    exit 1
fi

BENCHMARK_FOLDER="$1"
TIMEOUT_VAL="$2"
MYAPP_TIMEOUT="$3"
MYAPP_TIMEOUT_MS=$(( MYAPP_TIMEOUT * 1000 ))
SECOND_TIMEOUT=$((TIMEOUT_VAL - MYAPP_TIMEOUT))
RESULTS_FILE="results.txt"

MYAPP_CMD="taskset -c 0 ../build/myapp"
Z3_CMD="z3"
Q3B_CMD="taskset -c 1 ../build/external/q3b/q3b"
CVC5_CMD="/home/kouba/cvc5/cvc5/build/bin/cvc5"
BITW_CMD="bitwuzla"

declare -A COMMANDS
COMMANDS["z3"]="$Z3_CMD"
COMMANDS["q3b"]="$Q3B_CMD"
COMMANDS["cvc5"]="$CVC5_CMD"
COMMANDS["bitw"]="$BITW_CMD"

run_tool() {
    local timeout_val="$1"
    shift
    local start_time_ms=$(date +%s%3N)
    local output
    output=$(timeout "$timeout_val" $@ 2>&1)
    local exit_code=$?
    local end_time_ms=$(date +%s%3N)
    local duration_ms=$(( end_time_ms - start_time_ms ))
    if [ $exit_code -eq 124 ]; then
        echo "timeout $duration_ms"
    elif [ $exit_code -gt 0 ]; then
        echo "crash $duration_ms"
    else
        local res
        res=$(echo "$output" | tail -n 1)
        echo "$res $duration_ms"
    fi
}

evaluate_mytool() {
    local file="$1"
    local timeout="$2"

    rm *.smt2
    cp "$file" out.smt2
    cp "$file" in.smt2
    cp "$file" simplified.smt2
    local start_time_ms=$(date +%s%3N)
    local output
    output=$(timeout "$((timeout + 2))" $MYAPP_CMD --verbose:1 --timeout:$((timeout - 1)) "$file" 2>&1)
    local exit_code=$?
    local end_time_ms=$(date +%s%3N)
    local duration_ms=$(( end_time_ms - start_time_ms ))
    echo "$file $timeout $start_time_ms" >> log.txt
    echo "$output" >> log.txt
    if [ $exit_code -eq 124 ]; then
        echo "myapp_timeout $duration_ms"
    elif [ $exit_code -gt 0 ]; then
        echo "myapp_crash $duration_ms"
    else
        echo "succ $duration_ms"
    fi
}

SECONDARY_TOOLS=("z3" "cvc5" "bitw")
BASE_TOOLS=("z3" "q3b" "cvc5" "bitw")

cat "$BENCHMARK_FOLDER" | while read -r FILE; do
    echo "Processing benchmark: $FILE"

    declare -A TEMP_FILES
    declare -A PIDS

    ALL_TOOLS=()

    for tool in "${BASE_TOOLS[@]}"; do
        TEMP_FILES["$tool"]=$(mktemp)
        (result=$(run_tool "$TIMEOUT_VAL" "${COMMANDS[$tool]}" "$FILE"); echo "$result" > "${TEMP_FILES[$tool]}") &
        PIDS["$tool"]=$!
        ALL_TOOLS+=("$tool")
    done

    read MY_TOOL_RESULT MY_TOOL_DURATION < <(evaluate_mytool "$FILE" "$MYAPP_TIMEOUT")

    if [ $MY_TOOL_DURATION -gt $MYAPP_TIMEOUT_MS ]; then
        MY_TOOL_DURATION=$MYAPP_TIMEOUT_MS
    fi
    REMAINING_TIME=$(( TIMEOUT_VAL - ((MY_TOOL_DURATION + 500) / 1000) ))

    # for tool in "${SECONDARY_TOOLS[@]}"; do
    #     tool_name="myapp_parse+$tool"
    #     TEMP_FILES["$tool_name"]=$(mktemp)
    #     (result=$(run_tool "$REMAINING_TIME" "${COMMANDS[$tool]}" "in.smt2"); echo "$result" > "${TEMP_FILES[$tool_name]}") &
    #     PIDS["$tool_name"]=$!
    #     ALL_TOOLS+=("$tool_name")
    # done

    # for tool in "${SECONDARY_TOOLS[@]}"; do
    #     tool_name="myapp_simpl+$tool"
    #     TEMP_FILES["$tool_name"]=$(mktemp)
    #     (result=$(run_tool "$REMAINING_TIME" "${COMMANDS[$tool]}" "simplified.smt2"); echo "$result" > "${TEMP_FILES[$tool_name]}") &
    #     PIDS["$tool_name"]=$!
    #     ALL_TOOLS+=("$tool_name")
    # done

    for tool in "${SECONDARY_TOOLS[@]}"; do
        tool_name="myapp_full+$tool"
        TEMP_FILES["$tool_name"]=$(mktemp)
        (result=$(run_tool "$REMAINING_TIME" "${COMMANDS[$tool]}" "out.smt2"); echo "$result" > "${TEMP_FILES[$tool_name]}") &
        PIDS["$tool_name"]=$!
        ALL_TOOLS+=("$tool_name")
    done

    for pid in "${!PIDS[@]}"; do
        wait "${PIDS[$pid]}"
    done

    RES_LINE="$FILE, $MY_TOOL_RESULT, $MY_TOOL_DURATION"
    for tool in "${ALL_TOOLS[@]}"; do
        tool_name="$tool"
        read RESULT DUR < <(cat "${TEMP_FILES[$tool_name]}")
        RES_LINE+=", $RESULT, $DUR"
        rm "${TEMP_FILES[$tool_name]}"
    done

    echo "$RES_LINE" >> "$RESULTS_FILE"
done
