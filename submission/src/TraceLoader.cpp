// TraceLoader.cpp
#include "TraceLoader.h"

#include "Scheduler.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

namespace {

bool parseInt(const std::string& s, long& out) {
    try {
        size_t pos = 0;
        out = std::stol(s, &pos, 0);   // base 0: accepts 0x.. hex and decimal
        return pos == s.size();
    } catch (...) {
        return false;
    }
}

} // namespace

bool loadTrace(const std::string& path, TraceData& out) {
    std::ifstream in(path);
    if (!in) return false;

    std::string line;
    int lineNo = 0;
    while (std::getline(in, line)) {
        ++lineNo;
        size_t hash = line.find('#');
        if (hash != std::string::npos) line = line.substr(0, hash);

        std::istringstream ss(line);
        std::vector<std::string> tok;
        std::string t;
        while (ss >> t) tok.push_back(t);
        if (tok.empty()) continue;

        auto warn = [&](const std::string& why) {
            std::cerr << "Warning: " << path << ":" << lineNo << " skipped (" << why << ")\n";
            ++out.warnings;
        };

        std::string head = tok[0];
        std::transform(head.begin(), head.end(), head.begin(), ::toupper);

        if (head == "PRIORITY") {
            long pid, prio;
            if (tok.size() != 3 || !parseInt(tok[1], pid) || !parseInt(tok[2], prio)) {
                warn("expected: PRIORITY <pid> <priority>");
                continue;
            }
            out.processPriorities.push_back({static_cast<int>(pid), static_cast<int>(prio)});
            continue;
        }
        if (head == "THREAD") {
            long pid, tid, prio;
            if (tok.size() != 4 || !parseInt(tok[1], pid) || !parseInt(tok[2], tid) ||
                !parseInt(tok[3], prio)) {
                warn("expected: THREAD <pid> <tid> <priority>");
                continue;
            }
            out.threadPriorities.push_back({{static_cast<int>(pid), static_cast<int>(tid)},
                                            static_cast<int>(prio)});
            continue;
        }

        if (tok.size() != 4) { warn("expected: <pid> <thread_id> <address> <R|W>"); continue; }
        long pid, tid, addr;
        if (!parseInt(tok[0], pid) || !parseInt(tok[1], tid) || !parseInt(tok[2], addr)) {
            warn("bad number");
            continue;
        }
        std::string type = tok[3];
        std::transform(type.begin(), type.end(), type.begin(), ::toupper);
        if (type != "R" && type != "W") { warn("access type must be R or W"); continue; }
        if (pid < 0 || tid < 0 || tid > 999 || addr < 0) { warn("value out of range"); continue; }

        out.accesses.push_back({static_cast<int>(pid), static_cast<int>(tid),
                                static_cast<unsigned int>(addr), type == "W"});
    }
    return true;
}

void populateScheduler(const TraceData& trace, Scheduler& s) {
    for (const auto& pp : trace.processPriorities) s.setProcessPriority(pp.first, pp.second);
    for (const auto& tp : trace.threadPriorities)
        s.setThreadPriority(tp.first.first, tp.first.second, tp.second);
    for (const TraceAccess& a : trace.accesses) s.addAccess(a.pid, a.tid, a.address, a.isWrite);
}
