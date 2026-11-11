// CPU.cpp - CPU burst execution and switch accounting.
#include "CPU.h"

#include <iostream>

CPU::CPU() : cyclesExecuted(0), contextSwitches(0), threadSwitches(0) {}

SwitchType CPU::execute(Process& p, int threadId) {
    SwitchType s = SwitchType::None;
    if (lastPid != -1) {                    // the very first dispatch is not a switch
        if (p.pid != lastPid) { ++contextSwitches; s = SwitchType::Context; }
        else if (threadId != lastTid) { ++threadSwitches; s = SwitchType::Thread; }
    }
    lastPid = p.pid;
    lastTid = threadId;

    p.tick(threadId);                       // consume one unit of the CPU burst
    ++cyclesExecuted;
    ++cyclesPerThread[{p.pid, threadId}];
    return s;
}

void CPU::printStats() const { printStats(std::cout); }

void CPU::printStats(std::ostream& os) const {
    os << "  CPU\n"
       << "    cycles executed  : " << cyclesExecuted << "\n"
       << "    context switches : " << contextSwitches << "  (between processes)\n"
       << "    thread switches  : " << threadSwitches << "  (between threads of one process)\n"
       << "    cycles per thread:";
    for (const auto& kv : cyclesPerThread) {
        os << "  P" << kv.first.first << ".T" << kv.first.second << "=" << kv.second;
    }
    os << "\n";
}
