// Clock.cpp - Enhanced second-chance Clock with reference + modified bits.
#include "Clock.h"

#include <iomanip>
#include <stdexcept>

Clock::Clock(int frameCount) : frames(static_cast<size_t>(frameCount), {-1, -1, false, false}),
                               clockHand(0) {
    if (frameCount < 1) throw std::invalid_argument("need at least one frame");
}

int Clock::evictAndReplace(int pid, int pageNum, int& evictedPage, bool isModified) {
    evictedPage = -1;
    lastEvictedPid = -1;
    lastEvictionDirty = false;

    // Demand paging starts with empty memory: use free frames first.
    if (usedFrames < static_cast<int>(frames.size())) {
        int f = usedFrames++;
        frames[static_cast<size_t>(f)] = {pid, pageNum, true, isModified};
        ++freeFrameLoads;
        return f;
    }

    const size_t n = frames.size();
    size_t victim = n;   // "not found"

    while (victim == n) {
        // Pass A: look for (R=0, M=0); do not modify any bits.
        for (size_t step = 0; step < n; ++step) {
            FrameEntry& e = frames[clockHand];
            if (!e.referenced && !e.modified) { victim = clockHand; break; }
            clockHand = (clockHand + 1) % n;
            ++handMoves;
        }
        if (victim != n) break;

        // Pass B: look for (R=0, M=1); clear R bits as the hand passes.
        for (size_t step = 0; step < n; ++step) {
            FrameEntry& e = frames[clockHand];
            if (!e.referenced && e.modified) { victim = clockHand; break; }
            e.referenced = false;            // give it a second chance
            clockHand = (clockHand + 1) % n;
            ++handMoves;
        }
    }

    FrameEntry& v = frames[victim];
    evictedPage = v.pageNumber;
    lastEvictedPid = v.pid;
    lastEvictionDirty = v.modified;
    if (v.modified) ++dirtyEvictions; else ++cleanEvictions;

    v = {pid, pageNum, true, isModified};
    clockHand = (victim + 1) % n;            // hand moves past the new page
    ++handMoves;
    return static_cast<int>(victim);
}

void Clock::touch(int frameNum, bool isWrite) {
    FrameEntry& e = frames[static_cast<size_t>(frameNum)];
    e.referenced = true;
    if (isWrite) e.modified = true;
}

void Clock::printFrames(std::ostream& os) const {
    os << "  Frame table (hand at " << clockHand << ")\n";
    for (size_t i = 0; i < frames.size(); ++i) {
        const FrameEntry& e = frames[i];
        os << "    frame " << std::setw(2) << i << " : ";
        if (e.pid < 0) os << "free\n";
        else os << "P" << e.pid << " page " << std::setw(3) << e.pageNumber
                << "  R=" << e.referenced << " M=" << e.modified
                << (i == clockHand ? "   <- hand" : "") << "\n";
    }
}

void Clock::printStats(std::ostream& os) const {
    long evictions = cleanEvictions + dirtyEvictions;
    os << "  Advanced Clock (" << frames.size() << " frames)\n"
       << "    loads into free frames : " << freeFrameLoads << "\n"
       << "    evictions              : " << evictions << "\n"
       << "      clean (R=0,M=0)      : " << cleanEvictions << "\n"
       << "      dirty (write-back)   : " << dirtyEvictions << "\n"
       << "    clock hand moves       : " << handMoves << "\n";
}
