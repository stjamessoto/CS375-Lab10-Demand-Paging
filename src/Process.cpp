// Process.cpp - Process / thread bookkeeping.
#include "Process.h"

#include <stdexcept>

Process::Process(int pid, int priority)
    : pid(pid), priority(priority), cpuBurstRemaining(0), accessIndex(0) {}

void Process::addThread(int threadId, int threadPriority) {
    if (threadPriorities.count(threadId) == 0) threadOrder.push_back(threadId);
    threadPriorities[threadId] = threadPriority;
    threadAccesses[threadId];          // make sure the containers exist
    threadCursor.emplace(threadId, 0);
    threadBurstRemaining.emplace(threadId, 0);
}

void Process::addAccess(int threadId, unsigned int address, bool isWrite) {
    if (threadPriorities.count(threadId) == 0) addThread(threadId, priority);
    memoryAccesses.emplace_back(threadId, address);
    accessIsWrite.push_back(isWrite);
    threadAccesses[threadId].push_back(memoryAccesses.size() - 1);
    ++threadBurstRemaining[threadId];
    ++cpuBurstRemaining;
}

bool Process::hasMoreAccesses() const { return cpuBurstRemaining > 0; }

bool Process::hasMoreAccesses(int threadId) const {
    auto it = threadBurstRemaining.find(threadId);
    return it != threadBurstRemaining.end() && it->second > 0;
}

std::pair<int, unsigned int> Process::nextAccess() {
    if (accessIndex >= memoryAccesses.size()) throw std::out_of_range("no more accesses");
    return memoryAccesses[accessIndex++];
}

ThreadAccess Process::peekThreadAccess(int threadId) const {
    const std::vector<size_t>& list = threadAccesses.at(threadId);
    size_t cursor = threadCursor.at(threadId);
    if (cursor >= list.size()) throw std::out_of_range("thread has no more accesses");
    size_t idx = list[cursor];
    return {threadId, memoryAccesses[idx].second, accessIsWrite[idx]};
}

void Process::tick(int threadId) {
    if (!hasMoreAccesses(threadId)) return;
    ++threadCursor[threadId];
    --threadBurstRemaining[threadId];
    --cpuBurstRemaining;
}

int Process::threadBurst(int threadId) const {
    auto it = threadAccesses.find(threadId);
    return it == threadAccesses.end() ? 0 : static_cast<int>(it->second.size());
}
