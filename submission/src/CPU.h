// CPU.h - Executes one cycle of a thread and counts context / thread switches.
//
//   context switch : the next cycle belongs to a thread of a DIFFERENT process
//                    (address space changes)
//   thread switch  : the next cycle belongs to a different thread of the SAME
//                    process (cheap: same address space, TLB stays valid)
#ifndef CPU_H
#define CPU_H

#include "Process.h"

#include <map>
#include <ostream>
#include <utility>

enum class SwitchType { None, Context, Thread };

class CPU {
private:
    int cyclesExecuted;
    int contextSwitches;
    int threadSwitches;

    int lastPid = -1;
    int lastTid = -1;
    std::map<std::pair<int, int>, int> cyclesPerThread;   // (pid, tid) -> cycles

public:
    CPU();

    // Runs one cycle of (p, threadId). Returns what kind of switch, if any,
    // happened right before this cycle.
    SwitchType execute(Process& p, int threadId);

    int getCycles() const { return cyclesExecuted; }
    int getContextSwitches() const { return contextSwitches; }
    int getThreadSwitches() const { return threadSwitches; }

    void printStats() const;
    void printStats(std::ostream& os) const;
};

#endif
