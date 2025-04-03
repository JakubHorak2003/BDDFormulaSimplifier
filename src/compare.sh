
if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <benchmark_folder> <timeout>"
    exit 1
fi

BENCHMARK_FOLDER="$1"
TIMEOUT_VAL="$2"
HALF_TIMEOUT=$((TIMEOUT_VAL / 2))
RESULTS_FILE="results.txt"

MYAPP_CMD="../build/myapp"
Z3_CMD="z3"
Q3B_CMD="../build/external/q3b/q3b"
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

    rm *.smt2
    cp "$file" out.smt2
    cp "$file" in.smt2
    cp "$file" simplified.smt2
    local output
    output=$(timeout "$((half_timeout + 2))" taskset -c 0-2 "$MYAPP_CMD" --verbose:1 --timeout:$((half_timeout - 1)) "$file" 2>&1)
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

SECONDARY_TOOLS=("z3" "cvc5" "bitw")
BASE_TOOLS=("z3" "q3b" "cvc5" "bitw")

cat "$BENCHMARK_FOLDER" | while read -r FILE; do
    echo "Processing benchmark: $FILE"

    declare -A TEMP_FILES
    declare -A PIDS

    ALL_TOOLS=()

    for tool in "${BASE_TOOLS[@]}"; do
        echo "Launch $tool"
        TEMP_FILES["$tool"]=$(mktemp)
        (result=$(run_tool "$TIMEOUT_VAL" "${COMMANDS[$tool]}" "$FILE"); echo "$result" > "${TEMP_FILES[$tool]}") &
        PIDS["$tool"]=$!
        ALL_TOOLS+=("$tool")
    done

    MY_TOOL_RESULT=$(evaluate_mytool "$FILE" "$TIMEOUT_VAL")

    for tool in "${SECONDARY_TOOLS[@]}"; do
        tool_name="myapp_parse+$tool"
        echo "Launch $tool_name"
        TEMP_FILES["$tool_name"]=$(mktemp)
        (result=$(run_tool "$HALF_TIMEOUT" "${COMMANDS[$tool]}" "in.smt2"); echo "$result" > "${TEMP_FILES[$tool_name]}") &
        PIDS["$tool_name"]=$!
        ALL_TOOLS+=("$tool_name")
    done

    for tool in "${SECONDARY_TOOLS[@]}"; do
        tool_name="myapp_simpl+$tool"
        echo "Launch $tool_name"
        TEMP_FILES["$tool_name"]=$(mktemp)
        (result=$(run_tool "$HALF_TIMEOUT" "${COMMANDS[$tool]}" "simplified.smt2"); echo "$result" > "${TEMP_FILES[$tool_name]}") &
        PIDS["$tool_name"]=$!
        ALL_TOOLS+=("$tool_name")
    done

    for tool in "${SECONDARY_TOOLS[@]}"; do
        tool_name="myapp_full+$tool"
        echo "Launch $tool_name"
        TEMP_FILES["$tool_name"]=$(mktemp)
        (result=$(run_tool "$HALF_TIMEOUT" "${COMMANDS[$tool]}" "out.smt2"); echo "$result" > "${TEMP_FILES[$tool_name]}") &
        PIDS["$tool_name"]=$!
        ALL_TOOLS+=("$tool_name")
    done

    for pid in "${!PIDS[@]}"; do
        wait "${PIDS[$pid]}"
    done

    RES_LINE="$FILE, $MY_TOOL_RESULT"
    for tool in "${ALL_TOOLS[@]}"; do
        tool_name="$tool"
        RESULT=$(cat "${TEMP_FILES[$tool_name]}")
        RES_LINE+=", $RESULT"
        rm "${TEMP_FILES[$tool_name]}"
    done

    echo "$RES_LINE" >> "$RESULTS_FILE"
done
