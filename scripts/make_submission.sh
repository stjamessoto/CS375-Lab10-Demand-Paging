#!/usr/bin/env bash
# Builds submission/ with only what the handout asks for, plus what is
# needed to build and run it:
#
#   submission/
#     src/              all .cpp and .h files
#     input/trace.txt   modified trace with varied access patterns
#     output.log        auto-generated output log (all 4 policies + summary)
#     report.pdf        written report (summary table, timeline graphs, analysis)
#     Makefile          build + run
#     README.txt        how to build and run
#
# Re-run after any change:  make report && make submission
set -euo pipefail
cd "$(dirname "$0")/.."

make -s >/dev/null
./bin/vmsim --quiet >/dev/null          # regenerate output/trace_output.log
[ -f report/report.pdf ] || { echo "No report yet - run: make report"; exit 1; }

S=submission
rm -rf "$S"
mkdir -p "$S/src" "$S/input"

cp src/*.cpp src/*.h            "$S/src/"
cp input/trace.txt              "$S/input/"
cp output/trace_output.log      "$S/output.log"
cp report/report.pdf            "$S/report.pdf"

cat > "$S/Makefile" <<'EOF'
CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -O2

vmsim: src/*.cpp src/*.h
	$(CXX) $(CXXFLAGS) -o vmsim src/*.cpp

run: vmsim
	./vmsim

clean:
	rm -rf vmsim output
EOF

cat > "$S/README.txt" <<'EOF'
CS375 Lab 10 - Demand Paging + CPU Scheduling Simulator

Contents
  src/             all .cpp and .h files
  input/trace.txt  modified trace (original sample + sequential, random, localized patterns)
  output.log       auto-generated output log (FCFS, RR, Priority, MLFQ + summary table)
  report.pdf       report: summary table, timeline graphs, analysis

Build and run (g++ with C++17):
  make             (or: g++ -std=c++17 -O2 -o vmsim src/*.cpp)
  ./vmsim          runs all four policies on input/trace.txt and
                   regenerates the log as output/trace_output.log
  ./vmsim --help   options: --tlb N, --frames N, --policy fcfs|rr|priority|mlfq, ...
EOF

echo "Created $S/"

names="01_build 02_help 03_trace 04_page_table 05_tlb 06_clock 07_mlfq 08_threads
       09_ascii_timeline 10_summary 11_patterns 12_plotter 13_log_files"
shots=0; missing=""
for n in $names; do
    if [ -f "report/screenshots/$n.png" ] || [ -f "report/screenshots/$n.jpg" ]; then
        shots=$((shots + 1))
    else
        missing="$missing $n"
    fi
done
echo "Screenshots in report.pdf: $shots of 13"
if [ -n "$missing" ]; then
    echo "  missing:$missing"
    echo "  -> add them to report/screenshots/, then: make report && make submission"
fi
echo
find "$S" -type f | sort | sed 's/^/  /'
