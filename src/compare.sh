
if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <benchmark_folder> <timeout>"
    exit 1
fi

BENCHMARK_FOLDER="$1"
TIMEOUT_VAL="$2"
RESULTS_FILE="results.txt"

> "$RESULTS_FILE"

find "$BENCHMARK_FOLDER" -type f -name "*.smt2" | while read -r FILE; do
    echo "Processing benchmark: $FILE"

    Z3_OUTPUT=$(timeout "$TIMEOUT_VAL" z3 "$FILE" 2>&1)
    Z3_EXIT=$?
    if [ $Z3_EXIT -eq 124 ]; then
        Z3_RESULT="timeout"
    elif [ $Z3_EXIT -gt 0 ]; then
        Z3_RESULT="crash"
    else
        Z3_RESULT=$(echo "$Z3_OUTPUT" | tail -n 1)
    fi

    Q3B_OUTPUT=$(timeout "$TIMEOUT_VAL" ../build/external/q3b/q3b "$FILE" 2>&1)
    Q3B_EXIT=$?
    if [ $Q3B_EXIT -eq 124 ]; then
        Q3B_RESULT="timeout"
    elif [ $Q3B_EXIT -gt 0 ]; then
        Q3B_RESULT="crash"
    else
        Q3B_RESULT=$(echo "$Q3B_OUTPUT" | tail -n 1)
    fi

    CVC5_OUTPUT=$(timeout "$TIMEOUT_VAL" ~/cvc5/cvc5/build/bin/cvc5 "$FILE" 2>&1)
    CVC5_EXIT=$?
    if [ $CVC5_EXIT -eq 124 ]; then
        CVC5_RESULT="timeout"
    elif [ $CVC5_EXIT -gt 0 ]; then
        CVC5_RESULT="crash"
    else
        CVC5_RESULT=$(echo "$CVC5_OUTPUT" | tail -n 1)
    fi

    HALF_TIMEOUT=$(($TIMEOUT_VAL / 2))

    rm out.smt2
    cp "$FILE" out.smt2
    timeout "$HALF_TIMEOUT" ../build/myapp --timeout:$(($HALF_TIMEOUT - 3)) --verbose:1 "$FILE"
    MYAPP_EXIT=$?
    if [ $MYAPP_EXIT -gt 0 ] && [ $MYAPP_EXIT -ne 124 ]; then
        MYAPP_Z3_RESULT="crash"
        MYAPP_CVC5_RESULT="crash"
    else
        MYAPP_Z3_OUTPUT=$(timeout "$HALF_TIMEOUT" z3 out.smt2 2>&1)
        MYAPP_Z3_EXIT=$?
        if [ $MYAPP_Z3_EXIT -eq 124 ]; then
            MYAPP_Z3_RESULT="timeout"
        elif [ $MYAPP_Z3_EXIT -gt 0 ]; then
            MYAPP_Z3_RESULT="crash2"
        else
            MYAPP_Z3_RESULT=$(echo "$MYAPP_Z3_OUTPUT" | tail -n 1)
        fi

        MYAPP_CVC5_OUTPUT=$(timeout "$HALF_TIMEOUT" ~/cvc5/cvc5/build/bin/cvc5 out.smt2 2>&1)
        MYAPP_CVC5_EXIT=$?
        if [ $MYAPP_CVC5_EXIT -eq 124 ]; then
            MYAPP_CVC5_RESULT="timeout"
        elif [ $MYAPP_CVC5_EXIT -gt 0 ]; then
            MYAPP_CVC5_RESULT="crash2"
        else
            MYAPP_CVC5_RESULT=$(echo "$MYAPP_CVC5_OUTPUT" | tail -n 1)
        fi
    fi

    echo "$FILE, $Z3_RESULT, $Q3B_RESULT, $CVC5_RESULT, $MYAPP_Z3_RESULT, $MYAPP_CVC5_RESULT" >> "$RESULTS_FILE"
    echo "$Z3_RESULT, $Q3B_RESULT, $CVC5_RESULT, $MYAPP_Z3_RESULT, $MYAPP_CVC5_RESULT"
done
