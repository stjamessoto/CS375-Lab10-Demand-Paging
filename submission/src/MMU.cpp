// MMU.cpp - Address translation with demand paging.
#include "MMU.h"

#include <iomanip>

namespace {
int log2i(int v) {
    int b = 0;
    while ((1 << b) < v) ++b;
    return b;
}
} // namespace

MMU::MMU(const SimConfig& c)
    : cfg(c),
      tlb(c.tlbSize),
      pageTable(1 << (c.addressBits - log2i(c.pageSize))),
      clock(c.frameCount),
      offsetBits(log2i(c.pageSize)) {}

AccessResult MMU::access(int pid, unsigned int virtualAddress, bool isWrite) {
    AccessResult r;
    ++accesses;

    unsigned int mask = (1u << cfg.addressBits) - 1u;
    virtualAddress &= mask;
    r.pageNumber = static_cast<int>(virtualAddress >> offsetBits);
    r.offset     = static_cast<int>(virtualAddress & ((1u << offsetBits) - 1u));
    r.outerIndex = pageTable.outerIndex(r.pageNumber);
    r.innerIndex = pageTable.innerIndex(r.pageNumber);

    int frame = -1;
    if (tlb.lookup(pid, r.pageNumber, frame)) {
        // 1. TLB hit
        r.tlbHit = true;
        r.cost = cfg.tlbHitCost;
    } else {
        // 2. TLB miss: walk both levels of the page table
        r.cost = cfg.tlbHitCost + cfg.pageWalkCost;
        frame = pageTable.getFrame(pid, r.pageNumber);
        if (frame < 0) {
            // 3. Page fault: bring the page in, Clock chooses the frame
            r.pageFault = true;
            ++faults;
            pageTable.recordFault();
            r.cost += cfg.pageFaultCost;

            int victimPage = -1;
            frame = clock.evictAndReplace(pid, r.pageNumber, victimPage, isWrite);
            if (victimPage >= 0) {
                r.evictedPid  = clock.evictedPid();
                r.evictedPage = victimPage;
                r.writeBack   = clock.evictedWasDirty();
                pageTable.unloadPage(r.evictedPid, r.evictedPage);
                tlb.invalidate(r.evictedPid, r.evictedPage);
                if (r.writeBack) {
                    ++writeBacks;
                    r.cost += cfg.writeBackCost;
                }
            }
            pageTable.loadPage(pid, r.pageNumber, frame);
        }
        tlb.insert(pid, r.pageNumber, frame);
    }

    // Set R (and M on a write). On a fault, evictAndReplace already did this.
    if (!r.pageFault) clock.touch(frame, isWrite);

    r.frameNumber = frame;
    r.physicalAddress = (static_cast<unsigned int>(frame) << offsetBits) |
                        static_cast<unsigned int>(r.offset);
    totalCost += r.cost;
    return r;
}

void MMU::printStats(std::ostream& os) const {
    double faultRate = accesses == 0 ? 0.0 : 100.0 * static_cast<double>(faults) / static_cast<double>(accesses);
    double eat = accesses == 0 ? 0.0 : static_cast<double>(totalCost) / static_cast<double>(accesses);
    os << "  Memory accesses          : " << accesses << "\n"
       << "  Page faults              : " << faults << "  (" << std::fixed
       << std::setprecision(2) << faultRate << " %)\n"
       << "  Disk write-backs         : " << writeBacks << "\n"
       << "  Effective access cost    : " << std::setprecision(2) << eat
       << " cycles/access  (TLB " << cfg.tlbHitCost << ", walk +" << cfg.pageWalkCost
       << ", fault +" << cfg.pageFaultCost << ", write-back +" << cfg.writeBackCost << ")\n";
    tlb.printStats(os);
    pageTable.printStats(os);
    clock.printStats(os);
}
