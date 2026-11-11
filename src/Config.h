// Config.h - Simulation configuration (command-line flags and/or config file).
#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <vector>

struct SimConfig {
    // Memory system
    int tlbSize        = 8;     // number of TLB entries (0 = TLB disabled)
    int frameCount     = 16;    // number of physical frames
    int pageSize       = 256;   // bytes per page (power of two)
    int addressBits    = 16;    // virtual address width in bits
    bool tlbFlushOnContextSwitch = false;  // model an untagged TLB

    // Cost model used for the "effective memory cost" metric (cycles)
    int tlbHitCost     = 1;
    int pageWalkCost   = 2;     // one extra memory reference per page-table level
    int pageFaultCost  = 100;   // disk read
    int writeBackCost  = 100;   // disk write of a dirty victim

    // Scheduling
    std::vector<std::string> policies = {"fcfs", "rr", "priority", "mlfq"};
    int quantum        = 4;     // RR / Priority quantum, and MLFQ Q0 quantum
    int mlfqLevels     = 3;
    int agingThreshold = 30;    // MLFQ: ticks waiting before a thread is promoted

    // Files
    std::string tracePath  = "input/trace.txt";
    std::string outputDir  = "output";
    std::string tag;            // run label (defaults to the trace file stem)
    std::string summaryCsv;     // optional: append one row per policy here

    // Console output
    bool verbose   = false;     // echo every log line to the terminal
    bool timeline  = true;      // print the ASCII timeline
    int  timelineWidth = 100;   // ticks per timeline row
    int  timelineMaxTicks = 200;// limit the console timeline (log gets all)
    bool quiet     = false;     // only print the summary table
};

// Parses argv (and an optional --config file). Exits on --help or bad input.
SimConfig parseArguments(int argc, char* argv[]);

// Loads key=value pairs from a config file into cfg. Returns false on error.
bool loadConfigFile(const std::string& path, SimConfig& cfg);

void printUsage(const char* prog);

#endif
