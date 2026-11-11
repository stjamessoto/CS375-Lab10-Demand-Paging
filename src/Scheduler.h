// Scheduler.h - Thread scheduler: FCFS, Round Robin, Priority and MLFQ.
//
// The schedulable unit is a thread (pid, tid). One tick = one CPU cycle = one
// memory access of the running thread. All threads are ready at time 0, in
// order of their first appearance in the trace.
//
//   FCFS      : run each thread to completion, in arrival order.
//   RR        : quantum = 4 ticks, preempted threads go to the tail.
//   Priority  : always run the ready thread with the best (lowest) priority
//               number; equal priorities share the CPU round robin (quantum 4).
//   MLFQ      : 3 queues, quanta 4 / 8 / 16. Threads start in Q0 and are
//               demoted one level when they use a whole quantum. A thread that
//               waits >= agingThreshold ticks in Q1/Q2 is promoted one level.
#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "CPU.h"
#include "Config.h"
#include "Logger.h"
#include "MMU.h"
#include "Process.h"

#include <map>
#include <queue>
#include <string>
#include <vector>

enum class SchedulingPolicy { FCFS, RoundRobin, Priority, MLFQ };

std::string policyName(SchedulingPolicy p);        // "FCFS", "RR", ...
SchedulingPolicy policyFromString(const std::string& s);

// Per-thread scheduling statistics.
struct ThreadStats {
    int pid = 0;
    int tid = 0;
    int priority = 0;
    int burst = 0;          // total cycles needed
    int level = 0;          // MLFQ queue level
    int firstRun = -1;      // response time (arrival is 0)
    int finish = -1;        // completion time
    int agingSince = 0;     // when the thread last entered / was promoted in a queue
    int dispatches = 0;
    int demotions = 0;
    int promotions = 0;
    int tlbHits = 0;
    int tlbMisses = 0;
    int faults = 0;
    int waitTime() const { return finish < 0 ? 0 : finish - burst; }
    int turnaround() const { return finish; }
};

// Everything that happened in one tick (used for the CSV and the timelines).
struct TickEvent {
    int time;
    int pid;
    int tid;
    unsigned int address;
    bool isWrite;
    int pageNumber;
    int frameNumber;
    bool tlbHit;
    bool pageFault;
    bool writeBack;
    int evictedPid;
    int evictedPage;
    SwitchType switchType;
    int level;
};

class Scheduler {
private:
    SchedulingPolicy policy;
    std::map<int, Process> processes;
    std::vector<std::queue<int>> readyQueues;   // For MLFQ: multiple priority queues
    int quantum;

    SimConfig cfg;
    MMU mmu;
    CPU cpu;
    Logger* logger;

    std::map<int, ThreadStats> threads;         // key(pid, tid) -> stats
    std::vector<int> arrivalOrder;              // keys in order of first appearance
    std::vector<TickEvent> events;
    int time = 0;
    int current = -1;                           // key of running thread, -1 = idle
    int quantumUsed = 0;
    bool started = false;
    bool done = false;

    void start();
    void dispatch();
    void enqueue(int key);
    int  currentQuantum() const;
    void applyAging();
    void log(const std::string& s);

public:
    static int key(int pid, int tid) { return pid * 1000 + tid; }

    Scheduler(SchedulingPolicy policy, int tlbSize);
    Scheduler(SchedulingPolicy policy, const SimConfig& cfg, Logger* logger);

    void addProcessIfNotExists(int pid);
    Process& getProcess(int pid);
    void setProcessPriority(int pid, int priority);
    void setThreadPriority(int pid, int tid, int priority);
    void addAccess(int pid, int tid, unsigned int address, bool isWrite);

    void tick();   // Advance time, simulate scheduling and thread execution
    void run();    // tick() until every thread is finished
    bool finished() const { return done; }

    void adjustPriority(int pid, int threadId); // For MLFQ: demote (used full quantum) or promote (aging)

    SchedulingPolicy getPolicy() const { return policy; }
    int getTime() const { return time; }
    const MMU& getMMU() const { return mmu; }
    const CPU& getCPU() const { return cpu; }
    const std::vector<TickEvent>& getEvents() const { return events; }
    std::vector<ThreadStats> getThreadStats() const;   // in arrival order
};

#endif
