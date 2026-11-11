// main.cpp - Driver for the CS375 Lab 10 virtual memory + CPU simulator.
//
// For every requested scheduling policy it builds a fresh system (TLB,
// two-level page table, Clock frames, CPU, scheduler), replays the trace and
// writes:
//   output/logs/<tag>_<policy>.log       full per-tick log
//   output/events/<tag>_<policy>.csv     one row per tick (for plotter.py)
//   output/events/<tag>_<policy>_threads.csv
//   output/summary/<tag>.csv             one row per policy
//   output/summary/<tag>_table.tex       the same table for the LaTeX report
//   output/<tag>_output.log              all policy logs + summary in one file
#include "Config.h"
#include "Logger.h"
#include "Report.h"
#include "Scheduler.h"
#include "TraceLoader.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>

namespace {

void makeDir(const std::string& path) {
    std::string partial;
    std::stringstream ss(path);
    std::string part;
    if (!path.empty() && path[0] == '/') partial = "/";
    while (std::getline(ss, part, '/')) {
        if (part.empty()) continue;
        partial += part + "/";
        mkdir(partial.c_str(), 0755);   // ignore "already exists"
    }
}

std::string lowerStr(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

void printBanner(const SimConfig& cfg, const TraceData& trace, std::ostream& os) {
    int pageBits = 0;
    while ((1 << pageBits) < cfg.pageSize) ++pageBits;
    int vpnBits = cfg.addressBits - pageBits;
    int outer = (vpnBits + 1) / 2;
    os << "==============================================================\n"
       << " CS375 Lab 10 - Demand Paging + CPU Scheduling Simulator\n"
       << "==============================================================\n"
       << " Trace        : " << cfg.tracePath << "  (" << trace.accesses.size() << " accesses)\n"
       << " Address      : " << cfg.addressBits << "-bit virtual = " << outer << " outer + "
       << (vpnBits - outer) << " inner + " << pageBits << " offset bits\n"
       << " Page size    : " << cfg.pageSize << " bytes  (" << (1 << vpnBits) << " pages per process)\n"
       << " Frames       : " << cfg.frameCount << "  (advanced Clock, R + M bits)\n"
       << " TLB          : " << cfg.tlbSize << " entries, LRU"
       << (cfg.tlbFlushOnContextSwitch ? ", flushed on context switch" : ", tagged by PID") << "\n"
       << " Quantum      : " << cfg.quantum << "  (MLFQ: " << cfg.quantum << "/" << cfg.quantum * 2 << "/"
       << cfg.quantum * 4 << ", aging after " << cfg.agingThreshold << " ticks)\n"
       << " Policies     :";
    for (const std::string& p : cfg.policies) os << " " << p;
    os << "\n==============================================================\n";
}

} // namespace

int main(int argc, char* argv[]) {
    SimConfig cfg = parseArguments(argc, argv);

    TraceData trace;
    if (!loadTrace(cfg.tracePath, trace)) {
        std::cerr << "Error: cannot open trace file '" << cfg.tracePath << "'\n"
                  << "Run from the project folder, or pass --trace <file>.\n";
        return 1;
    }
    if (trace.accesses.empty()) {
        std::cerr << "Error: trace '" << cfg.tracePath << "' contains no memory accesses\n";
        return 1;
    }

    const std::string logDir = cfg.outputDir + "/logs";
    const std::string eventDir = cfg.outputDir + "/events";
    const std::string sumDir = cfg.outputDir + "/summary";
    makeDir(logDir);
    makeDir(eventDir);
    makeDir(sumDir);

    if (!cfg.quiet) printBanner(cfg, trace, std::cout);

    std::vector<RunResult> results;
    for (const std::string& pol : cfg.policies) {
        SchedulingPolicy policy = policyFromString(pol);
        std::string base = cfg.tag + "_" + lowerStr(policyName(policy));
        Logger logger(logDir + "/" + base + ".log", cfg.verbose && !cfg.quiet);

        std::ostringstream head;
        printBanner(cfg, trace, head);
        logger.stream() << head.str();
        logger.log("Policy: " + policyName(policy));
        logger.log("--------------------------------------------------------------");

        if (!cfg.quiet && cfg.verbose)
            std::cout << "\n---------------- " << policyName(policy) << " : per-tick log ----------------\n";

        Scheduler sched(policy, cfg, &logger);
        populateScheduler(trace, sched);
        sched.run();

        RunResult r = collectResult(sched, cfg);
        results.push_back(r);

        std::ostringstream stats;
        stats << "\n#################### " << policyName(policy) << " ####################\n";
        sched.getCPU().printStats(stats);
        sched.getMMU().printStats(stats);
        sched.getMMU().getClock().printFrames(stats);
        printThreadTable(r, stats);

        std::ostringstream timeline;
        printAsciiTimeline(sched, timeline, cfg.timelineWidth, 0);   // full timeline in the log
        logger.stream() << stats.str() << timeline.str();

        if (!cfg.quiet) {
            std::cout << stats.str();
            if (cfg.timeline) printAsciiTimeline(sched, std::cout, cfg.timelineWidth, cfg.timelineMaxTicks);
            std::cout << "\n  Log written to    " << logger.getPath() << "\n";
        }

        writeEventsCsv(sched, eventDir + "/" + base + ".csv");
        writeThreadsCsv(r, eventDir + "/" + base + "_threads.csv");
    }

    printSummaryTable(results, std::cout);

    std::string sumCsv = sumDir + "/" + cfg.tag + ".csv";
    std::string sumTex = sumDir + "/" + cfg.tag + "_table.tex";
    writeSummaryCsv(results, sumCsv, false);
    writeLatexTable(results, sumTex);
    if (!cfg.summaryCsv.empty()) writeSummaryCsv(results, cfg.summaryCsv, true);

    // One combined log: every policy's log followed by the summary table.
    std::string combined = cfg.outputDir + "/" + cfg.tag + "_output.log";
    {
        std::ofstream all(combined);
        for (const std::string& pol : cfg.policies) {
            std::ifstream part(logDir + "/" + cfg.tag + "_" +
                               lowerStr(policyName(policyFromString(pol))) + ".log");
            all << part.rdbuf() << "\n";
        }
        printSummaryTable(results, all);
    }

    std::cout << "\n Output files:\n"
              << "   log     : " << combined << "  (all policies + summary)\n"
              << "   logs    : " << logDir << "/" << cfg.tag << "_<policy>.log\n"
              << "   events  : " << eventDir << "/" << cfg.tag << "_<policy>.csv\n"
              << "   summary : " << sumCsv << "  and  " << sumTex << "\n";
    if (!cfg.summaryCsv.empty()) std::cout << "   appended: " << cfg.summaryCsv << "\n";
    return 0;
}
