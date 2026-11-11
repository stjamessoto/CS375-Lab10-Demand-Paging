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
