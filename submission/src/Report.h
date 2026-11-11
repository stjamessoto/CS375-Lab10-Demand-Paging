// Report.h - Metrics, summary tables, CSV / LaTeX export and ASCII timelines.
#ifndef REPORT_H
#define REPORT_H

#include "Config.h"
#include "Scheduler.h"

#include <ostream>
#include <string>
#include <vector>

struct RunResult {
    std::string tag;
    std::string policy;
    int tlbSize = 0;
    int frames = 0;
    int pageSize = 0;
    long accesses = 0;
    long tlbHits = 0;
    long tlbMisses = 0;
    double tlbHitRate = 0;       // %
    long pageFaults = 0;
    double pageFaultRate = 0;    // %
    long writeBacks = 0;
    int contextSwitches = 0;
    int threadSwitches = 0;
    int totalTicks = 0;
    double avgWait = 0;
    double avgTurnaround = 0;
    double avgResponse = 0;
    int maxWait = 0;
    double jainFairness = 0;     // 1.0 = perfectly fair
    double effectiveAccessCost = 0;
    std::vector<ThreadStats> threads;
};

RunResult collectResult(const Scheduler& s, const SimConfig& cfg);

// Per-thread table (burst, wait, turnaround, response, TLB, faults ...).
void printThreadTable(const RunResult& r, std::ostream& os);

// One row per policy: the "Summary table of performance metrics".
void printSummaryTable(const std::vector<RunResult>& results, std::ostream& os);

// Text-based timeline graph: CPU bursts per thread, TLB hits/misses, page
// faults, context and thread switches.
void printAsciiTimeline(const Scheduler& s, std::ostream& os, int width, int maxTicks);

// Files used by plotter.py and the LaTeX report.
bool writeEventsCsv(const Scheduler& s, const std::string& path);
bool writeThreadsCsv(const RunResult& r, const std::string& path);
bool writeSummaryCsv(const std::vector<RunResult>& results, const std::string& path, bool append);
bool writeLatexTable(const std::vector<RunResult>& results, const std::string& path);

#endif
