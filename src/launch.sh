BENCHMARK_FILE="$1"
TIMEOUT_VAL="$2"
MYAPP_TIMEOUT="$3"
N_PROCESSES="$4"

python3 splitbench.py "$BENCHMARK_FILE" "$N_PROCESSES"

for (( i=0; i<N_PROCESSES; i++ )); do
    rm -rf "tmp$i"
    mkdir "tmp$i"
    ./compare.sh "all_files_$i.txt" "$TIMEOUT_VAL" "$MYAPP_TIMEOUT" "$i" &
    sleep 5
done
