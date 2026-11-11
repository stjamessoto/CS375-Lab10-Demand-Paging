// MMU.h - Memory management unit: ties the TLB, the two-level page table and
// the Clock replacement policy together to translate one virtual address.
//
// Translation path for every access:
//   1. TLB lookup                 -> hit: done
//   2. walk the two-level table   -> present: refill the TLB, done
//   3. page fault                 -> Clock picks a frame (maybe evicting a
//                                    victim, writing it back if dirty), the
//                                    page table and TLB are updated.
#ifndef MMU_H
#define MMU_H

#include "Clock.h"
#include "Config.h"
#include "TLB.h"
#include "TwoLevelPageTable.h"

#include <ostream>

struct AccessResult {
    int  pageNumber   = 0;
    int  outerIndex   = 0;
    int  innerIndex   = 0;
    int  offset       = 0;
    int  frameNumber  = -1;
    unsigned int physicalAddress = 0;
    bool tlbHit       = false;
    bool pageFault    = false;
    int  evictedPid   = -1;    // -1 if a free frame was used
    int  evictedPage  = -1;
    bool writeBack    = false; // victim was dirty
    int  cost         = 0;     // modelled cycles for this access
};

class MMU {
private:
    SimConfig cfg;
    TLB tlb;
    TwoLevelPageTable pageTable;
    Clock clock;
    int offsetBits;

    long accesses = 0;
    long faults = 0;
    long writeBacks = 0;
    long totalCost = 0;

public:
    explicit MMU(const SimConfig& cfg);

    AccessResult access(int pid, unsigned int virtualAddress, bool isWrite);
    void flushTLB() { tlb.flush(); }

    long getAccesses() const { return accesses; }
    long getFaults() const { return faults; }
    long getWriteBacks() const { return writeBacks; }
    long getTotalCost() const { return totalCost; }
    const TLB& getTLB() const { return tlb; }
    const TwoLevelPageTable& getPageTable() const { return pageTable; }
    const Clock& getClock() const { return clock; }

    void printStats(std::ostream& os) const;
};

#endif
