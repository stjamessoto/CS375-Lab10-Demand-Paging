// TwoLevelPageTable.cpp - Two-level page table implementation.
#include "TwoLevelPageTable.h"

#include <iostream>
#include <stdexcept>

TwoLevelPageTable::TwoLevelPageTable(int pageCount) : pageCount(pageCount) {
    if (pageCount < 2 || (pageCount & (pageCount - 1)) != 0) {
        throw std::invalid_argument("page count must be a power of two >= 2");
    }
    int bits = 0;
    while ((1 << bits) < pageCount) ++bits;
    outerBits = (bits + 1) / 2;        // e.g. 8 page bits -> 4 outer + 4 inner
    innerBits = bits - outerBits;
}

bool TwoLevelPageTable::isInMemory(int pid, int pageNum) const {
    return memoryPages.count({pid, pageNum}) > 0;
}

int TwoLevelPageTable::getFrame(int pid, int pageNum) const {
    ++walks;
    // Level 1: find the second-level table for this outer index.
    auto outer = firstLevel.find({pid, outerIndex(pageNum)});
    if (outer == firstLevel.end()) return -1;            // no inner table yet
    // Level 2: find the frame for this inner index.
    auto inner = secondLevel.find({outer->second, innerIndex(pageNum)});
    if (inner == secondLevel.end()) return -1;
    return inner->second;                                // -1 if invalid
}

void TwoLevelPageTable::loadPage(int pid, int pageNum, int frameNum) {
    std::pair<int, int> outerKey{pid, outerIndex(pageNum)};
    auto outer = firstLevel.find(outerKey);
    if (outer == firstLevel.end()) {
        // First page in this range: allocate a new second-level table.
        outer = firstLevel.emplace(outerKey, nextTableId++).first;
    }
    secondLevel[{outer->second, innerIndex(pageNum)}] = frameNum;
    memoryPages.insert({pid, pageNum});
    ++pagesLoaded;
}

void TwoLevelPageTable::unloadPage(int pid, int pageNum) {
    auto outer = firstLevel.find({pid, outerIndex(pageNum)});
    if (outer != firstLevel.end()) {
        auto inner = secondLevel.find({outer->second, innerIndex(pageNum)});
        if (inner != secondLevel.end()) inner->second = -1;   // valid bit = 0
    }
    if (memoryPages.erase({pid, pageNum}) > 0) ++pagesEvicted;
}

void TwoLevelPageTable::printStats() const { printStats(std::cout); }

void TwoLevelPageTable::printStats(std::ostream& os) const {
    long entriesPerInner = 1L << innerBits;
    long twoLevelEntries = static_cast<long>(firstLevel.size()) * entriesPerInner;

    // Count processes to compare against a flat (single-level) table.
    std::set<int> pids;
    for (const auto& kv : firstLevel) pids.insert(kv.first.first);
    long flatEntries = static_cast<long>(pids.size()) * pageCount;

    os << "  Two-level page table (" << outerBits << " outer bits + " << innerBits
       << " inner bits, " << pageCount << " pages per process)\n"
       << "    page-table walks       : " << walks << "\n"
       << "    page faults            : " << pageFaults << "\n"
       << "    pages loaded / evicted : " << pagesLoaded << " / " << pagesEvicted << "\n"
       << "    resident pages now     : " << memoryPages.size() << "\n"
       << "    second-level tables    : " << firstLevel.size()
       << "  (" << twoLevelEntries << " inner entries allocated vs "
       << flatEntries << " for flat tables)\n";
}
