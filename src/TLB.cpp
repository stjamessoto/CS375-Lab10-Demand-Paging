// TLB.cpp - LRU TLB implementation.
#include "TLB.h"

#include <iomanip>
#include <iostream>

TLB::TLB(int size) : currentTime(0), maxSize(size < 0 ? 0 : size) {
    tlbEntries.reserve(maxSize);
}

bool TLB::lookup(int pid, int pageNumber, int& frameNumber) {
    ++currentTime;
    for (TLBEntry& e : tlbEntries) {
        if (e.pid == pid && e.pageNumber == pageNumber) {
            e.lastUsed = currentTime;        // LRU: refresh on every hit
            frameNumber = e.frameNumber;
            ++hits;
            return true;
        }
    }
    ++misses;
    return false;
}

void TLB::insert(int pid, int pageNumber, int frameNumber) {
    if (maxSize == 0) return;                // TLB disabled
    ++currentTime;

    // Already present (e.g. re-inserted after a walk): just refresh it.
    for (TLBEntry& e : tlbEntries) {
        if (e.pid == pid && e.pageNumber == pageNumber) {
            e.frameNumber = frameNumber;
            e.lastUsed = currentTime;
            return;
        }
    }

    if (static_cast<int>(tlbEntries.size()) < maxSize) {
        tlbEntries.push_back({pid, pageNumber, frameNumber, currentTime});
        return;
    }

    // Full: replace the least recently used entry.
    size_t victim = 0;
    for (size_t i = 1; i < tlbEntries.size(); ++i) {
        if (tlbEntries[i].lastUsed < tlbEntries[victim].lastUsed) victim = i;
    }
    tlbEntries[victim] = {pid, pageNumber, frameNumber, currentTime};
    ++evictions;
}

void TLB::invalidate(int pid, int pageNumber) {
    for (size_t i = 0; i < tlbEntries.size(); ++i) {
        if (tlbEntries[i].pid == pid && tlbEntries[i].pageNumber == pageNumber) {
            tlbEntries.erase(tlbEntries.begin() + static_cast<long>(i));
            ++invalidations;
            return;
        }
    }
}

void TLB::flush() {
    if (!tlbEntries.empty()) ++flushes;
    tlbEntries.clear();
}

double TLB::hitRate() const {
    long total = hits + misses;
    return total == 0 ? 0.0 : 100.0 * static_cast<double>(hits) / static_cast<double>(total);
}

void TLB::printStats() const { printStats(std::cout); }

void TLB::printStats(std::ostream& os) const {
    os << "  TLB (" << maxSize << " entries, LRU)\n"
       << "    lookups        : " << (hits + misses) << "\n"
       << "    hits / misses  : " << hits << " / " << misses << "\n"
       << "    hit rate       : " << std::fixed << std::setprecision(2) << hitRate() << " %\n"
       << "    LRU evictions  : " << evictions << "\n"
       << "    invalidations  : " << invalidations << "  (entries removed when their page was evicted)\n"
       << "    flushes        : " << flushes << "\n";
}
