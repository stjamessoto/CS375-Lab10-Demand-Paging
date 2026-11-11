// Scheduler.cpp - FCFS, Round Robin, Priority and MLFQ thread scheduling.
#include "Scheduler.h"

#include <algorithm>
#include <cstdio>
#include <stdexcept>

namespace {

SimConfig configWithTlb(int tlbSize) {
    SimConfig c;
    c.tlbSize = tlbSize;
    return c;
}

std::string threadName(int pid, int tid) {
    return "P" + std::to_string(pid) + ".T" + std::to_string(tid);
}

std::string stamp(int t) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "[t=%5d] ", t);
    return buf;
}

std::string hex(unsigned int v, int width) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "0x%0*X", width, v);
    return buf;
}

} // namespace

std::string policyName(SchedulingPolicy p) {
    switch (p) {
        case SchedulingPolicy::FCFS:       return "FCFS";
        case SchedulingPolicy::RoundRobin: return "RR";
        case SchedulingPolicy::Priority:   return "Priority";
        case SchedulingPolicy::MLFQ:       return "MLFQ";
    }
    return "?";
}

SchedulingPolicy policyFromString(const std::string& s) {
    if (s == "fcfs") return SchedulingPolicy::FCFS;
    if (s == "rr") return SchedulingPolicy::RoundRobin;
    if (s == "priority") return SchedulingPolicy::Priority;
    if (s == "mlfq") return SchedulingPolicy::MLFQ;
    throw std::invalid_argument("unknown policy " + s);
}

Scheduler::Scheduler(SchedulingPolicy policy, int tlbSize)
    : Scheduler(policy, configWithTlb(tlbSize), nullptr) {}

Scheduler::Scheduler(SchedulingPolicy p, const SimConfig& c, Logger* lg)
    : policy(p),
      readyQueues(p == SchedulingPolicy::MLFQ ? static_cast<size_t>(c.mlfqLevels) : 1),
      quantum(c.quantum),
      cfg(c),
      mmu(c),
      logger(lg) {}

void Scheduler::log(const std::string& s) {
    if (logger) logger->log(s);
}

// ---------------------------------------------------------------- setup

void Scheduler::addProcessIfNotExists(int pid) {
    if (processes.count(pid) == 0) processes.emplace(pid, Process(pid, 5));
}

Process& Scheduler::getProcess(int pid) {
    auto it = processes.find(pid);
    if (it == processes.end()) throw std::out_of_range("no process " + std::to_string(pid));
    return it->second;
}

void Scheduler::setProcessPriority(int pid, int priority) {
    addProcessIfNotExists(pid);
    getProcess(pid).priority = priority;
}

void Scheduler::setThreadPriority(int pid, int tid, int priority) {
    addProcessIfNotExists(pid);
    getProcess(pid).addThread(tid, priority);
}

void Scheduler::addAccess(int pid, int tid, unsigned int address, bool isWrite) {
    addProcessIfNotExists(pid);
    Process& p = getProcess(pid);
    int k = key(pid, tid);
    if (std::find(arrivalOrder.begin(), arrivalOrder.end(), k) == arrivalOrder.end()) {
        arrivalOrder.push_back(k);
    }
    p.addAccess(tid, address, isWrite);
}

// ---------------------------------------------------------------- core

void Scheduler::start() {
    started = true;
    for (int k : arrivalOrder) {
        int pid = k / 1000, tid = k % 1000;
        Process& p = getProcess(pid);
        if (p.threadBurst(tid) == 0) continue;
        ThreadStats ts;
        ts.pid = pid;
        ts.tid = tid;
        ts.priority = p.threadPriorities[tid];
        ts.burst = p.threadBurst(tid);
        threads[k] = ts;
        enqueue(k);
        log(stamp(0) + "ARRIVE   " + threadName(pid, tid) + "  burst=" + std::to_string(ts.burst) +
            " priority=" + std::to_string(ts.priority));
    }
}

void Scheduler::enqueue(int k) {
    ThreadStats& ts = threads[k];
    ts.agingSince = time;
    size_t level = policy == SchedulingPolicy::MLFQ ? static_cast<size_t>(ts.level) : 0;
    readyQueues[level].push(k);
}

int Scheduler::currentQuantum() const {
    switch (policy) {
        case SchedulingPolicy::FCFS:       return 1 << 30;           // never preempt
        case SchedulingPolicy::RoundRobin: return quantum;
        case SchedulingPolicy::Priority:   return quantum;
        case SchedulingPolicy::MLFQ:       return quantum << threads.at(current).level; // 4, 8, 16
    }
    return quantum;
}

void Scheduler::dispatch() {
    int next = -1;
    int level = 0;

    if (policy == SchedulingPolicy::Priority) {
        // Pick the best priority; ties keep queue (FIFO) order -> round robin.
        std::queue<int>& q = readyQueues[0];
        std::vector<int> items;
        while (!q.empty()) { items.push_back(q.front()); q.pop(); }
        size_t best = items.size();
        for (size_t i = 0; i < items.size(); ++i) {
            if (best == items.size() || threads[items[i]].priority < threads[items[best]].priority) best = i;
        }
        for (size_t i = 0; i < items.size(); ++i) {
            if (i == best) next = items[i]; else q.push(items[i]);
        }
    } else {
        for (size_t l = 0; l < readyQueues.size(); ++l) {
            if (!readyQueues[l].empty()) {
                next = readyQueues[l].front();
                readyQueues[l].pop();
                level = static_cast<int>(l);
                break;
            }
        }
    }

    current = next;
    quantumUsed = 0;
    if (current < 0) return;

    ThreadStats& ts = threads[current];
    if (ts.firstRun < 0) ts.firstRun = time;
    ++ts.dispatches;

    std::string msg = stamp(time) + "DISPATCH " + threadName(ts.pid, ts.tid);
    if (policy == SchedulingPolicy::MLFQ)
        msg += "  from Q" + std::to_string(level) + " (quantum " + std::to_string(currentQuantum()) + ")";
    else if (policy == SchedulingPolicy::Priority)
        msg += "  priority " + std::to_string(ts.priority) + " (quantum " + std::to_string(quantum) + ")";
    else if (policy == SchedulingPolicy::RoundRobin)
        msg += "  (quantum " + std::to_string(quantum) + ")";
    else
        msg += "  (runs to completion)";
    log(msg);
}

void Scheduler::adjustPriority(int pid, int threadId) {
    if (policy != SchedulingPolicy::MLFQ) return;
    int k = key(pid, threadId);
    ThreadStats& ts = threads.at(k);
    int lowest = static_cast<int>(readyQueues.size()) - 1;

    if (k == current) {
        // Used its whole quantum -> CPU bound -> demote one level.
        if (ts.level < lowest) {
            ++ts.level;
            ++ts.demotions;
            log(stamp(time) + "DEMOTE   " + threadName(pid, threadId) + "  Q" +
                std::to_string(ts.level - 1) + " -> Q" + std::to_string(ts.level) +
                " (used full quantum)");
        }
    } else if (ts.level > 0) {
        // Waited too long -> aging -> promote one level.
        --ts.level;
        ++ts.promotions;
        log(stamp(time) + "PROMOTE  " + threadName(pid, threadId) + "  Q" +
            std::to_string(ts.level + 1) + " -> Q" + std::to_string(ts.level) +
            " (aging: waited " + std::to_string(time - ts.agingSince) + " ticks)");
    }
}

void Scheduler::applyAging() {
    if (policy != SchedulingPolicy::MLFQ) return;
    for (size_t l = 1; l < readyQueues.size(); ++l) {
        std::queue<int> keep;
        std::vector<int> promoted;
        while (!readyQueues[l].empty()) {
            int k = readyQueues[l].front();
            readyQueues[l].pop();
            if (time - threads[k].agingSince >= cfg.agingThreshold) promoted.push_back(k);
            else keep.push(k);
        }
        readyQueues[l] = keep;
        for (int k : promoted) {
            adjustPriority(k / 1000, k % 1000);
            enqueue(k);                       // joins the tail of the higher queue
        }
    }
}

void Scheduler::tick() {
    if (done) return;
    if (!started) start();
    if (current < 0) dispatch();
    if (current < 0) {
        done = true;
        log(stamp(time) + "IDLE     all threads finished");
        return;
    }

    ThreadStats& ts = threads[current];
    Process& p = getProcess(ts.pid);
    ThreadAccess a = p.peekThreadAccess(ts.tid);

    SwitchType sw = cpu.execute(p, ts.tid);   // one CPU cycle of this thread's burst
    if (sw == SwitchType::Context) {
        log(stamp(time) + "CONTEXT SWITCH -> P" + std::to_string(ts.pid) +
            (cfg.tlbFlushOnContextSwitch ? "  (TLB flushed)" : ""));
        if (cfg.tlbFlushOnContextSwitch) mmu.flushTLB();
    } else if (sw == SwitchType::Thread) {
        log(stamp(time) + "THREAD SWITCH  -> " + threadName(ts.pid, ts.tid) + " (same address space)");
    }

    AccessResult r = mmu.access(ts.pid, a.address, a.isWrite);

    if (r.tlbHit) ++ts.tlbHits; else ++ts.tlbMisses;
    if (r.pageFault) ++ts.faults;

    events.push_back({time, ts.pid, ts.tid, a.address, a.isWrite, r.pageNumber, r.frameNumber,
                      r.tlbHit, r.pageFault, r.writeBack, r.evictedPid, r.evictedPage, sw, ts.level});

    int hexWidth = (cfg.addressBits + 3) / 4;
    std::string line = stamp(time) + threadName(ts.pid, ts.tid) + "  " + (a.isWrite ? "W " : "R ") +
                       hex(a.address, hexWidth) + "  page " + std::to_string(r.pageNumber) +
                       " (outer " + std::to_string(r.outerIndex) + ", inner " +
                       std::to_string(r.innerIndex) + ") off " + hex(static_cast<unsigned>(r.offset), 2) +
                       " | TLB " + (r.tlbHit ? "HIT " : "MISS");
    if (r.pageFault) {
        line += " | PAGE FAULT -> frame " + std::to_string(r.frameNumber);
        if (r.evictedPage >= 0) {
            line += " (evicted P" + std::to_string(r.evictedPid) + " page " + std::to_string(r.evictedPage) +
                    (r.writeBack ? ", dirty -> write-back)" : ", clean)");
        } else {
            line += " (free frame)";
        }
    } else if (!r.tlbHit) {
        line += " | page table hit, frame " + std::to_string(r.frameNumber);
    }
    line += " | PA " + hex(r.physicalAddress, hexWidth);
    log(line);

    ++time;
    ++quantumUsed;

    if (!p.hasMoreAccesses(ts.tid)) {
        ts.finish = time;
        log(stamp(time) + "FINISH   " + threadName(ts.pid, ts.tid) + "  turnaround=" +
            std::to_string(ts.turnaround()) + " wait=" + std::to_string(ts.waitTime()));
        current = -1;
    } else if (quantumUsed >= currentQuantum()) {
        log(stamp(time) + "PREEMPT  " + threadName(ts.pid, ts.tid) + "  quantum expired");
        adjustPriority(ts.pid, ts.tid);       // MLFQ demotion (no-op for others)
        int k = current;
        current = -1;
        enqueue(k);
    }

    applyAging();
}

void Scheduler::run() {
    while (!done) tick();
}

std::vector<ThreadStats> Scheduler::getThreadStats() const {
    std::vector<ThreadStats> out;
    for (int k : arrivalOrder) {
        auto it = threads.find(k);
        if (it != threads.end()) out.push_back(it->second);
    }
    return out;
}
