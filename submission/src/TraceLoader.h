// TraceLoader.h - Reads input/trace.txt.
//
// Trace format (one item per line, '#' starts a comment):
//
//   <pid> <thread_id> <address> <R|W>     memory access, address in hex or decimal
//   PRIORITY <pid> <priority>             optional: process priority (lower = higher)
//   THREAD <pid> <thread_id> <priority>   optional: thread priority
//
// Threads without a THREAD line inherit their process priority (default 5).
#ifndef TRACE_LOADER_H
#define TRACE_LOADER_H

#include <string>
#include <vector>

class Scheduler;

struct TraceAccess {
    int pid;
    int tid;
    unsigned int address;
    bool isWrite;
};

struct TraceData {
    std::vector<TraceAccess> accesses;
    std::vector<std::pair<int, int>> processPriorities;              // (pid, priority)
    std::vector<std::pair<std::pair<int, int>, int>> threadPriorities; // ((pid, tid), priority)
    int warnings = 0;
};

// Returns false if the file cannot be opened.
bool loadTrace(const std::string& path, TraceData& out);

// Feeds a loaded trace into a fresh scheduler.
void populateScheduler(const TraceData& trace, Scheduler& s);

#endif
