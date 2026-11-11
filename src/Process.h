// Process.h - A process made of lightweight threads. Every thread has its own
// stream of memory accesses (its CPU burst) and its own priority.
#ifndef PROCESS_H
#define PROCESS_H

#include <cstddef>
#include <map>
#include <utility>
#include <vector>

struct ThreadAccess {
    int threadId;
    unsigned int address;
    bool isWrite;
};

class Process {
public:
    int pid;
    int priority;                 // lower number = higher priority
    int cpuBurstRemaining;        // accesses (cycles) left for the whole process
    std::vector<std::pair<int, unsigned int>> memoryAccesses; // (thread_id, address), trace order
    size_t accessIndex;           // cursor for nextAccess() (trace order)
    std::map<int, int> threadPriorities;   // Thread ID to priority

    // Per-thread bookkeeping (each thread runs its own accesses independently)
    std::vector<bool> accessIsWrite;                    // parallel to memoryAccesses
    std::map<int, std::vector<size_t>> threadAccesses;  // tid -> indexes into memoryAccesses
    std::map<int, size_t> threadCursor;                 // tid -> next index in threadAccesses[tid]
    std::map<int, int> threadBurstRemaining;            // tid -> accesses left
    std::vector<int> threadOrder;                       // tids in order of first appearance

    Process() : Process(0, 5) {}
    Process(int pid, int priority);

    void addThread(int threadId, int threadPriority);
    void addAccess(int threadId, unsigned int address, bool isWrite);

    bool hasMoreAccesses() const;
    bool hasMoreAccesses(int threadId) const;

    std::pair<int, unsigned int> nextAccess();     // Returns (thread_id, address) in trace order
    ThreadAccess peekThreadAccess(int threadId) const;  // next access of one thread

    // One CPU cycle for threadId: consumes that thread's next access.
    void tick(int threadId);

    int threadBurst(int threadId) const;           // total accesses of the thread
};

#endif
