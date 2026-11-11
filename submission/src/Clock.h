// Clock.h - Advanced (enhanced second-chance) Clock page replacement.
//
// Every frame has a reference bit R and a modified (dirty) bit M. When memory
// is full the hand looks for a victim in this order:
//
//   class 1  (R=0, M=0)  not used recently, clean  -> best victim, no disk write
//   class 2  (R=0, M=1)  not used recently, dirty  -> must be written back
//
//   Pass A: sweep once looking for class 1 without touching any bits.
//   Pass B: sweep once looking for class 2, clearing R on every frame passed.
//   Repeat - after pass B every R bit is 0, so the next pass A/B must succeed.
//
// Clean pages are therefore preferred, which reduces disk I/O.
#ifndef CLOCK_H
#define CLOCK_H

#include <ostream>
#include <vector>

struct FrameEntry {
    int pid;
    int pageNumber;
    bool referenced;
    bool modified;   // Tracks if page was modified
};

class Clock {
private:
    std::vector<FrameEntry> frames;
    size_t clockHand;

    int usedFrames = 0;
    int lastEvictedPid = -1;
    bool lastEvictionDirty = false;

    // statistics
    long cleanEvictions = 0;
    long dirtyEvictions = 0;    // = disk write-backs
    long handMoves = 0;
    long freeFrameLoads = 0;

public:
    explicit Clock(int frameCount);

    // Places (pid, pageNum) into a frame and returns that frame number.
    // If a page had to be evicted, evictedPage receives its page number
    // (and evictedPid() its owner); otherwise evictedPage is -1.
    int evictAndReplace(int pid, int pageNum, int& evictedPage, bool isModified);

    // Called on every hit: sets R, and sets M on a write.
    void touch(int frameNum, bool isWrite);

    int evictedPid() const { return lastEvictedPid; }
    bool evictedWasDirty() const { return lastEvictionDirty; }
    long getDirtyEvictions() const { return dirtyEvictions; }
    long getCleanEvictions() const { return cleanEvictions; }
    int frameCount() const { return static_cast<int>(frames.size()); }
    const FrameEntry& frame(int f) const { return frames[static_cast<size_t>(f)]; }

    void printFrames(std::ostream& os) const;
    void printStats(std::ostream& os) const;
};

#endif
