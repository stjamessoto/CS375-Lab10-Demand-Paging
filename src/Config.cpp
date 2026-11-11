// Config.cpp - Command-line and config-file parsing.
#include "Config.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

namespace {

std::string trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

bool isPowerOfTwo(int v) { return v > 0 && (v & (v - 1)) == 0; }

int toInt(const std::string& key, const std::string& value) {
    try {
        size_t pos = 0;
        int v = std::stoi(value, &pos, 0);
        if (pos != value.size()) throw std::invalid_argument("trailing");
        return v;
    } catch (...) {
        std::cerr << "Error: '" << key << "' expects an integer, got '" << value << "'\n";
        std::exit(1);
    }
}

bool toBool(const std::string& value) {
    std::string v = lower(value);
    return v == "1" || v == "true" || v == "yes" || v == "on";
}

std::vector<std::string> parsePolicies(const std::string& value) {
    std::vector<std::string> out;
    std::stringstream ss(lower(value));
    std::string item;
    while (std::getline(ss, item, ',')) {
        item = trim(item);
        if (item.empty()) continue;
        if (item == "all") return {"fcfs", "rr", "priority", "mlfq"};
        if (item == "roundrobin" || item == "round-robin") item = "rr";
        if (item == "prio") item = "priority";
        if (item != "fcfs" && item != "rr" && item != "priority" && item != "mlfq") {
            std::cerr << "Error: unknown policy '" << item
                      << "' (use fcfs, rr, priority, mlfq or all)\n";
            std::exit(1);
        }
        out.push_back(item);
    }
    if (out.empty()) {
        std::cerr << "Error: no scheduling policy given\n";
        std::exit(1);
    }
    return out;
}

// Applies one setting; shared by the config file and the command line.
bool applySetting(const std::string& rawKey, const std::string& value, SimConfig& cfg) {
    std::string key = lower(rawKey);
    std::replace(key.begin(), key.end(), '-', '_');

    if (key == "tlb" || key == "tlb_size")            cfg.tlbSize = toInt(key, value);
    else if (key == "frames" || key == "frame_count") cfg.frameCount = toInt(key, value);
    else if (key == "page_size")                      cfg.pageSize = toInt(key, value);
    else if (key == "address_bits" || key == "addr_bits") cfg.addressBits = toInt(key, value);
    else if (key == "tlb_flush")                      cfg.tlbFlushOnContextSwitch = toBool(value);
    else if (key == "quantum")                        cfg.quantum = toInt(key, value);
    else if (key == "aging" || key == "aging_threshold") cfg.agingThreshold = toInt(key, value);
    else if (key == "policy" || key == "policies")    cfg.policies = parsePolicies(value);
    else if (key == "trace")                          cfg.tracePath = value;
    else if (key == "out" || key == "output_dir")     cfg.outputDir = value;
    else if (key == "tag")                            cfg.tag = value;
    else if (key == "summary" || key == "summary_csv") cfg.summaryCsv = value;
    else if (key == "page_fault_cost")                cfg.pageFaultCost = toInt(key, value);
    else if (key == "write_back_cost")                cfg.writeBackCost = toInt(key, value);
    else if (key == "page_walk_cost")                 cfg.pageWalkCost = toInt(key, value);
    else if (key == "timeline")                       cfg.timeline = toBool(value);
    else if (key == "timeline_width")                 cfg.timelineWidth = toInt(key, value);
    else if (key == "timeline_ticks")                 cfg.timelineMaxTicks = toInt(key, value);
    else if (key == "verbose")                        cfg.verbose = toBool(value);
    else if (key == "quiet")                          cfg.quiet = toBool(value);
    else return false;
    return true;
}

void validate(const SimConfig& cfg) {
    bool ok = true;
    if (cfg.tlbSize < 0)   { std::cerr << "Error: --tlb must be >= 0\n"; ok = false; }
    if (cfg.frameCount < 1){ std::cerr << "Error: --frames must be >= 1\n"; ok = false; }
    if (!isPowerOfTwo(cfg.pageSize)) {
        std::cerr << "Error: --page-size must be a power of two\n"; ok = false;
    }
    if (cfg.addressBits < 4 || cfg.addressBits > 30) {
        std::cerr << "Error: --address-bits must be between 4 and 30\n"; ok = false;
    }
    if (isPowerOfTwo(cfg.pageSize) && cfg.pageSize >= (1 << cfg.addressBits)) {
        std::cerr << "Error: page size must be smaller than the address space\n"; ok = false;
    }
    if (cfg.quantum < 1)   { std::cerr << "Error: --quantum must be >= 1\n"; ok = false; }
    if (cfg.agingThreshold < 1) { std::cerr << "Error: --aging must be >= 1\n"; ok = false; }
    if (!ok) std::exit(1);
}

} // namespace

void printUsage(const char* prog) {
    std::cout <<
        "Usage: " << prog << " [options]\n"
        "\n"
        "Virtual memory + CPU scheduling simulator (CS375 Lab 10)\n"
        "\n"
        "Input / output:\n"
        "  --trace FILE        trace file to simulate        (default input/trace.txt)\n"
        "  --config FILE       load key=value settings from a file (e.g. config/sim.conf)\n"
        "  --out DIR           output directory              (default output)\n"
        "  --tag NAME          label for output files        (default: trace file name)\n"
        "  --summary FILE      append one CSV row per policy to FILE\n"
        "\n"
        "Memory system:\n"
        "  --tlb N             TLB entries, LRU eviction     (default 8, 0 = no TLB)\n"
        "  --frames N          physical frames               (default 16)\n"
        "  --page-size N       bytes per page, power of two  (default 256)\n"
        "  --address-bits N    virtual address width         (default 16)\n"
        "  --tlb-flush         flush the TLB on every context switch (untagged TLB)\n"
        "\n"
        "Scheduling:\n"
        "  --policy LIST       fcfs, rr, priority, mlfq or all; comma separated (default all)\n"
        "  --quantum N         RR/Priority quantum and MLFQ Q0 quantum (default 4)\n"
        "  --aging N           MLFQ aging threshold in ticks (default 30)\n"
        "\n"
        "Console:\n"
        "  --verbose           print every log line to the terminal as well\n"
        "  --quiet             print only the final comparison table\n"
        "  --no-timeline       do not print the ASCII timeline\n"
        "  --timeline-ticks N  ticks of timeline to print on screen (default 200, 0 = all)\n"
        "  --timeline-width N  ticks per timeline row (default 100)\n"
        "  -h, --help          show this help\n"
        "\n"
        "Examples:\n"
        "  " << prog << "                                  # all four policies on input/trace.txt\n"
        "  " << prog << " --policy mlfq --tlb 4 --verbose   # one policy, small TLB, full log\n"
        "  " << prog << " --config config/sim.conf          # settings from a file\n";
}

bool loadConfigFile(const std::string& path, SimConfig& cfg) {
    std::ifstream in(path);
    if (!in) {
        std::cerr << "Error: cannot open config file '" << path << "'\n";
        return false;
    }
    std::string line;
    int lineNo = 0;
    while (std::getline(in, line)) {
        ++lineNo;
        size_t hash = line.find('#');
        if (hash != std::string::npos) line = line.substr(0, hash);
        line = trim(line);
        if (line.empty()) continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) {
            std::cerr << "Warning: " << path << ":" << lineNo << " ignored (expected key=value)\n";
            continue;
        }
        std::string key = trim(line.substr(0, eq));
        std::string value = trim(line.substr(eq + 1));
        if (!applySetting(key, value, cfg)) {
            std::cerr << "Warning: " << path << ":" << lineNo << " unknown key '" << key << "'\n";
        }
    }
    return true;
}

SimConfig parseArguments(int argc, char* argv[]) {
    SimConfig cfg;

    // First pass: a --config file provides the defaults, later flags override it.
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--config" && i + 1 < argc) {
            if (!loadConfigFile(argv[i + 1], cfg)) std::exit(1);
        }
    }

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto need = [&](const std::string& flag) -> std::string {
            if (i + 1 >= argc) {
                std::cerr << "Error: " << flag << " needs a value\n";
                std::exit(1);
            }
            return argv[++i];
        };

        if (a == "-h" || a == "--help") { printUsage(argv[0]); std::exit(0); }
        else if (a == "--config")       { ++i; }                       // handled above
        else if (a == "--verbose" || a == "-v") cfg.verbose = true;
        else if (a == "--quiet" || a == "-q")   cfg.quiet = true;
        else if (a == "--no-timeline")          cfg.timeline = false;
        else if (a == "--tlb-flush")            cfg.tlbFlushOnContextSwitch = true;
        else if (a.rfind("--", 0) == 0) {
            std::string key = a.substr(2);
            std::string value;
            size_t eq = key.find('=');
            if (eq != std::string::npos) {            // --tlb=8 form
                value = key.substr(eq + 1);
                key = key.substr(0, eq);
            } else {
                value = need(a);
            }
            if (!applySetting(key, value, cfg)) {
                std::cerr << "Error: unknown option '" << a << "' (see --help)\n";
                std::exit(1);
            }
        } else {
            std::cerr << "Error: unexpected argument '" << a << "' (see --help)\n";
            std::exit(1);
        }
    }

    validate(cfg);

    if (cfg.tag.empty()) {
        std::string base = cfg.tracePath;
        size_t slash = base.find_last_of("/\\");
        if (slash != std::string::npos) base = base.substr(slash + 1);
        size_t dot = base.find_last_of('.');
        if (dot != std::string::npos) base = base.substr(0, dot);
        cfg.tag = base.empty() ? "run" : base;
    }
    return cfg;
}
