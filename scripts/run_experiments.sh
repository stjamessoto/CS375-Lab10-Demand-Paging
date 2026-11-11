#!/usr/bin/env bash
# Runs every experiment needed for the report and fills output/.
#
#   1. main trace (input/trace.txt), all four policies, default settings
#   2. the three access-pattern traces, all four policies
#   3. TLB size sweep (1..32 entries) for every trace and policy
#   4. physical frame sweep (4..32 frames) on the main trace
#   5. tagged vs. flushed TLB on the main trace
#
# Usage:  bash scripts/run_experiments.sh        (from the project folder)
set -euo pipefail
cd "$(dirname "$0")/.."

BIN=./bin/vmsim
[ -x "$BIN" ] || make

TRACES="trace trace_sequential trace_random trace_localized"
SUM=output/summary
mkdir -p "$SUM"
rm -f "$SUM/tlb_sweep.csv" "$SUM/frames_sweep.csv" "$SUM/tlb_flush.csv" "$SUM/all_patterns.csv.tmp"

echo "== 1+2. Policies on each trace (default: TLB 8, 16 frames) =="
for t in $TRACES; do
    $BIN --trace "input/$t.txt" --quiet --summary "$SUM/all_patterns.csv.tmp" | sed -n '/PERFORMANCE SUMMARY/,/^ Fault%/p'
done
mv "$SUM/all_patterns.csv.tmp" "$SUM/all_patterns.csv"

echo "== 3. TLB size sweep =="
for t in $TRACES; do
    for n in 1 2 4 8 16 32; do
        $BIN --trace "input/$t.txt" --tlb "$n" --quiet --no-timeline \
             --out output/sweep --tag "${t}_tlb${n}" --summary "$SUM/tlb_sweep.csv" >/dev/null
    done
    echo "   $t done"
done

echo "== 4. Frame count sweep (main trace) =="
for f in 4 8 12 16 24 32; do
    $BIN --trace input/trace.txt --frames "$f" --quiet --no-timeline \
         --out output/sweep --tag "trace_frames${f}" --summary "$SUM/frames_sweep.csv" >/dev/null
done
echo "   done"

echo "== 5. Tagged vs flushed TLB (main trace) =="
$BIN --trace input/trace.txt --quiet --no-timeline --out output/sweep --tag trace_tagged \
     --summary "$SUM/tlb_flush.csv" >/dev/null
$BIN --trace input/trace.txt --quiet --no-timeline --out output/sweep --tag trace_flushed \
     --tlb-flush --summary "$SUM/tlb_flush.csv" | sed -n '/PERFORMANCE SUMMARY/,/^ Fault%/p'

# Re-run the main trace last so output/summary/trace.csv holds the default run.
$BIN --trace input/trace.txt --quiet >/dev/null

echo
echo "All results are in output/summary/*.csv  (logs: output/logs, sweep runs: output/sweep)"
echo "Next: make plots"
