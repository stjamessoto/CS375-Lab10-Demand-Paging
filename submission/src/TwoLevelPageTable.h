// TwoLevelPageTable.h - Two-level (hierarchical) page table.
//
// A virtual page number (VPN) is split into an outer index and an inner index:
//
//      VPN = [ outer | inner ]
//
//   firstLevel  : (pid, outer)              -> id of a second-level table
//   secondLevel : (secondLevelId, inner)    -> frame number (-1 = not present)
//
// Second-level tables are allocated on demand, the first time a page inside
// their range faults in. This is what saves memory compared to a flat table.
#ifndef TWO_LEVEL_PAGE_TABLE_H
#define TWO_LEVEL_PAGE_TABLE_H

#include <map>
#include <ostream>
#include <set>
#include <utility>

class TwoLevelPageTable {
private:
    std::map<std::pair<int, int>, int> firstLevel;   // (pid, outer_page) -> second-level table id
    std::map<std::pair<int, int>, int> secondLevel;  // (table id, inner_page) -> frame#
    std::set<std::pair<int, int>> memoryPages;       // (pid, page) currently resident

    int pageCount;    // virtual pages per address space
    int innerBits;    // bits used by the inner index
    int outerBits;    // bits used by the outer index
    int nextTableId = 0;

    // statistics
    mutable long walks = 0;
    long pageFaults = 0;
    long pagesLoaded = 0;
    long pagesEvicted = 0;

public:
    explicit TwoLevelPageTable(int pageCount);

    int outerIndex(int pageNum) const { return pageNum >> innerBits; }
    int innerIndex(int pageNum) const { return pageNum & ((1 << innerBits) - 1); }
    int getOuterBits() const { return outerBits; }
    int getInnerBits() const { return innerBits; }

    bool isInMemory(int pid, int pageNum) const;

    // Walks both levels. Returns the frame, or -1 if the page is not resident.
    int getFrame(int pid, int pageNum) const;

    // Handles the page-table side of a page fault: allocates the second-level
    // table if needed and stores the frame number.
    void loadPage(int pid, int pageNum, int frameNum);

    // Marks a page as no longer resident (its frame was taken by Clock).
    void unloadPage(int pid, int pageNum);

    void recordFault() { ++pageFaults; }
    long getWalks() const { return walks; }
    int secondLevelTables() const { return static_cast<int>(firstLevel.size()); }

    void printStats() const;
    void printStats(std::ostream& os) const;
};

#endif
