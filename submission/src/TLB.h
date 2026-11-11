// TLB.h - Translation Lookaside Buffer with configurable size and LRU eviction.
#ifndef TLB_H
#define TLB_H

#include <ostream>
#include <vector>

class TLBEntry {
public:
    int pid;
    int pageNumber;
    int frameNumber;
    int lastUsed;   // logical timestamp for LRU
};

class TLB {
private:
    std::vector<TLBEntry> tlbEntries;
    int currentTime;
    int maxSize;

    // statistics
    long hits = 0;
    long misses = 0;
    long evictions = 0;
    long invalidations = 0;
    long flushes = 0;

public:
    explicit TLB(int size);

    // Returns true on a hit and stores the frame in frameNumber.
    bool lookup(int pid, int pageNumber, int& frameNumber);

    // Inserts (or refreshes) a translation; evicts the least recently used
    // entry when the TLB is full.
    void insert(int pid, int pageNumber, int frameNumber);

    // Removes the entry for (pid, page) - called when that page is evicted
    // from physical memory so the TLB never holds a stale translation.
    void invalidate(int pid, int pageNumber);

    // Removes every entry (used when modelling an untagged TLB).
    void flush();

    int size() const { return maxSize; }
    long getHits() const { return hits; }
    long getMisses() const { return misses; }
    long getEvictions() const { return evictions; }
    double hitRate() const;

    void printStats() const;
    void printStats(std::ostream& os) const;
};

#endif
