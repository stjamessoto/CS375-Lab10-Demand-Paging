// Report.cpp - Metrics, tables and timelines.
#include "Report.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace {

std::string fmt(double v, int prec = 2) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(prec) << v;
    return ss.str();
}

std::string threadLabel(int pid, int tid) {
    return "P" + std::to_string(pid) + ".T" + std::to_string(tid);
}

std::string switchName(SwitchType s) {
    switch (s) {
        case SwitchType::Context: return "context";
        case SwitchType::Thread:  return "thread";
        default:                  return "none";
    }
}

bool fileHasContent(const std::string& path) {
    std::ifstream in(path);
    return in && in.peek() != std::ifstream::traits_type::eof();
}

} // namespace

RunResult collectResult(const Scheduler& s, const SimConfig& cfg) {
    RunResult r;
    const MMU& mmu = s.getMMU();
    r.tag = cfg.tag;
    r.policy = policyName(s.getPolicy());
    r.tlbSize = cfg.tlbSize;
    r.frames = cfg.frameCount;
    r.pageSize = cfg.pageSize;
    r.accesses = mmu.getAccesses();
    r.tlbHits = mmu.getTLB().getHits();
    r.tlbMisses = mmu.getTLB().getMisses();
    r.tlbHitRate = mmu.getTLB().hitRate();
    r.pageFaults = mmu.getFaults();
    r.pageFaultRate = r.accesses ? 100.0 * static_cast<double>(r.pageFaults) / static_cast<double>(r.accesses) : 0;
    r.writeBacks = mmu.getWriteBacks();
    r.contextSwitches = s.getCPU().getContextSwitches();
    r.threadSwitches = s.getCPU().getThreadSwitches();
    r.totalTicks = s.getTime();
    r.effectiveAccessCost = r.accesses ? static_cast<double>(mmu.getTotalCost()) / static_cast<double>(r.accesses) : 0;
    r.threads = s.getThreadStats();

    double sumW = 0, sumT = 0, sumR = 0, sumX = 0, sumX2 = 0;
    for (const ThreadStats& t : r.threads) {
        sumW += t.waitTime();
        sumT += t.turnaround();
        sumR += t.firstRun;
        r.maxWait = std::max(r.maxWait, t.waitTime());
        // Normalised progress: share of its time in the system the thread ran.
        double x = t.turnaround() > 0 ? static_cast<double>(t.burst) / t.turnaround() : 1.0;
        sumX += x;
        sumX2 += x * x;
    }
    double n = static_cast<double>(r.threads.size());
    if (n > 0) {
        r.avgWait = sumW / n;
        r.avgTurnaround = sumT / n;
        r.avgResponse = sumR / n;
        r.jainFairness = sumX2 > 0 ? (sumX * sumX) / (n * sumX2) : 1.0;
    }
    return r;
}

void printThreadTable(const RunResult& r, std::ostream& os) {
    os << "  Per-thread results (" << r.policy << ")\n"
       << "    Thread    Prio  Burst  Response  Wait  Turnaround  Dispatches  TLB hit%  Faults\n"
       << "    --------  ----  -----  --------  ----  ----------  ----------  --------  ------\n";
    for (const ThreadStats& t : r.threads) {
        int lookups = t.tlbHits + t.tlbMisses;
        double hr = lookups ? 100.0 * t.tlbHits / lookups : 0.0;
        os << "    " << std::left << std::setw(8) << threadLabel(t.pid, t.tid) << std::right
           << std::setw(6) << t.priority << std::setw(7) << t.burst << std::setw(10) << t.firstRun
           << std::setw(6) << t.waitTime() << std::setw(12) << t.turnaround()
           << std::setw(12) << t.dispatches << std::setw(10) << fmt(hr, 1)
           << std::setw(8) << t.faults;
        if (t.demotions || t.promotions)
            os << "   (MLFQ: " << t.demotions << " demotions, " << t.promotions << " promotions)";
        os << "\n";
    }
}

void printSummaryTable(const std::vector<RunResult>& results, std::ostream& os) {
    if (results.empty()) return;
    const RunResult& f = results.front();
    os << "\n=====================================================================================================\n"
       << " PERFORMANCE SUMMARY  trace=" << f.tag << "  TLB=" << f.tlbSize << " entries  frames="
       << f.frames << "  page=" << f.pageSize << " B  accesses=" << f.accesses << "\n"
       << "=====================================================================================================\n"
       << " Policy    TLB hit%  Fault%  Faults  WriteBk  CtxSw  ThrSw  AvgWait  AvgTurn  AvgResp  MaxWait  Fairness\n"
       << " --------  --------  ------  ------  -------  -----  -----  -------  -------  -------  -------  --------\n";
    for (const RunResult& r : results) {
        os << " " << std::left << std::setw(8) << r.policy << std::right
           << std::setw(10) << fmt(r.tlbHitRate) << std::setw(8) << fmt(r.pageFaultRate)
           << std::setw(8) << r.pageFaults << std::setw(9) << r.writeBacks
           << std::setw(7) << r.contextSwitches << std::setw(7) << r.threadSwitches
           << std::setw(9) << fmt(r.avgWait, 1) << std::setw(9) << fmt(r.avgTurnaround, 1)
           << std::setw(9) << fmt(r.avgResponse, 1) << std::setw(9) << r.maxWait
           << std::setw(10) << fmt(r.jainFairness, 3) << "\n";
    }
    os << "-----------------------------------------------------------------------------------------------------\n"
       << " Fault% = page faults / accesses.  Fairness = Jain's index of (burst / turnaround), 1.000 = fair.\n"
       << " CtxSw = switch to another process, ThrSw = switch between threads of the same process.\n";
}

void printAsciiTimeline(const Scheduler& s, std::ostream& os, int width, int maxTicks) {
    const std::vector<TickEvent>& ev = s.getEvents();
    std::vector<ThreadStats> threads = s.getThreadStats();
    int total = static_cast<int>(ev.size());
    int shown = (maxTicks > 0) ? std::min(total, maxTicks) : total;
    if (width < 10) width = 10;
    bool mlfq = s.getPolicy() == SchedulingPolicy::MLFQ;

    os << "\n  ASCII timeline - " << policyName(s.getPolicy()) << "  (ticks 0-" << (shown - 1)
       << " of " << total << ")\n"
       << "  Legend: '=' TLB hit   'm' TLB miss (page table hit)   'F' page fault   "
          "'W' page fault + dirty write-back\n"
       << "          '.' ready/waiting   'C' context switch   't' thread switch"
       << (mlfq ? "   Queue row = MLFQ level of the running thread" : "") << "\n";

    for (int start = 0; start < shown; start += width) {
        int end = std::min(start + width, shown);
        // axis
        std::string axis(static_cast<size_t>(end - start), ' ');
        std::string ruler(static_cast<size_t>(end - start), '-');
        for (int t = start; t < end; ++t) {
            if (t % 10 == 0) {
                std::string n = std::to_string(t);
                for (size_t i = 0; i < n.size() && (t - start) + static_cast<int>(i) < end - start; ++i)
                    axis[static_cast<size_t>(t - start) + i] = n[i];
                ruler[static_cast<size_t>(t - start)] = '|';
            }
        }
        os << "\n  " << std::left << std::setw(9) << "tick" << axis << "\n"
           << "  " << std::setw(9) << "" << ruler << std::right << "\n";

        for (const ThreadStats& th : threads) {
            std::string row(static_cast<size_t>(end - start), ' ');
            for (int t = start; t < end; ++t) {
                const TickEvent& e = ev[static_cast<size_t>(t)];
                char c;
                if (e.pid == th.pid && e.tid == th.tid) {
                    c = e.pageFault ? (e.writeBack ? 'W' : 'F') : (e.tlbHit ? '=' : 'm');
                } else {
                    c = (t < th.finish) ? '.' : ' ';
                }
                row[static_cast<size_t>(t - start)] = c;
            }
            os << "  " << std::left << std::setw(9) << threadLabel(th.pid, th.tid) << std::right << row << "\n";
        }

        std::string sw(static_cast<size_t>(end - start), ' ');
        std::string lvl(static_cast<size_t>(end - start), ' ');
        for (int t = start; t < end; ++t) {
            const TickEvent& e = ev[static_cast<size_t>(t)];
            if (e.switchType == SwitchType::Context) sw[static_cast<size_t>(t - start)] = 'C';
            else if (e.switchType == SwitchType::Thread) sw[static_cast<size_t>(t - start)] = 't';
            lvl[static_cast<size_t>(t - start)] = static_cast<char>('0' + e.level);
        }
        os << "  " << std::left << std::setw(9) << "Switch" << std::right << sw << "\n";
        if (mlfq) os << "  " << std::left << std::setw(9) << "Queue" << std::right << lvl << "\n";
    }
    if (shown < total)
        os << "\n  ... " << (total - shown) << " more ticks in the log / events CSV"
           << " (use --timeline-ticks 0 to print everything)\n";
}

bool writeEventsCsv(const Scheduler& s, const std::string& path) {
    std::ofstream out(path);
    if (!out) return false;
    out << "time,pid,tid,address,rw,page,frame,tlb,page_fault,write_back,evicted_pid,evicted_page,switch,mlfq_level\n";
    for (const TickEvent& e : s.getEvents()) {
        out << e.time << ',' << e.pid << ',' << e.tid << ",0x" << std::hex << std::uppercase
            << e.address << std::dec << ',' << (e.isWrite ? 'W' : 'R') << ',' << e.pageNumber << ','
            << e.frameNumber << ',' << (e.tlbHit ? "hit" : "miss") << ',' << (e.pageFault ? 1 : 0)
            << ',' << (e.writeBack ? 1 : 0) << ',' << e.evictedPid << ',' << e.evictedPage << ','
            << switchName(e.switchType) << ',' << e.level << '\n';
    }
    return true;
}

bool writeThreadsCsv(const RunResult& r, const std::string& path) {
    std::ofstream out(path);
    if (!out) return false;
    out << "policy,pid,tid,priority,burst,response,wait,turnaround,dispatches,tlb_hits,tlb_misses,faults,demotions,promotions\n";
    for (const ThreadStats& t : r.threads) {
        out << r.policy << ',' << t.pid << ',' << t.tid << ',' << t.priority << ',' << t.burst << ','
            << t.firstRun << ',' << t.waitTime() << ',' << t.turnaround() << ',' << t.dispatches << ','
            << t.tlbHits << ',' << t.tlbMisses << ',' << t.faults << ',' << t.demotions << ','
            << t.promotions << '\n';
    }
    return true;
}

bool writeSummaryCsv(const std::vector<RunResult>& results, const std::string& path, bool append) {
    bool header = !append || !fileHasContent(path);
    std::ofstream out(path, append ? std::ios::app : std::ios::trunc);
    if (!out) return false;
    if (header)
        out << "tag,policy,tlb_size,frames,page_size,accesses,tlb_hits,tlb_misses,tlb_hit_rate,"
               "page_faults,page_fault_rate,write_backs,context_switches,thread_switches,total_ticks,"
               "avg_wait,avg_turnaround,avg_response,max_wait,jain_fairness,eff_access_cost\n";
    for (const RunResult& r : results) {
        out << r.tag << ',' << r.policy << ',' << r.tlbSize << ',' << r.frames << ',' << r.pageSize << ','
            << r.accesses << ',' << r.tlbHits << ',' << r.tlbMisses << ',' << fmt(r.tlbHitRate) << ','
            << r.pageFaults << ',' << fmt(r.pageFaultRate) << ',' << r.writeBacks << ','
            << r.contextSwitches << ',' << r.threadSwitches << ',' << r.totalTicks << ','
            << fmt(r.avgWait) << ',' << fmt(r.avgTurnaround) << ',' << fmt(r.avgResponse) << ','
            << r.maxWait << ',' << fmt(r.jainFairness, 3) << ',' << fmt(r.effectiveAccessCost) << '\n';
    }
    return true;
}

bool writeLatexTable(const std::vector<RunResult>& results, const std::string& path) {
    std::ofstream out(path);
    if (!out || results.empty()) return false;
    const RunResult& f = results.front();
    std::string tag = f.tag;
    std::string safeTag;
    for (char c : tag) { if (c == '_') safeTag += "\\_"; else safeTag += c; }
    out << "% Auto-generated by vmsim - do not edit by hand.\n"
        << "\\begin{tabular}{lrrrrrrrr}\n\\toprule\n"
        << "Policy & TLB hit \\% & Fault \\% & Faults & Write-backs & Ctx sw & Thr sw & Avg wait & Fairness \\\\\n"
        << "\\midrule\n";
    for (const RunResult& r : results) {
        out << r.policy << " & " << fmt(r.tlbHitRate) << " & " << fmt(r.pageFaultRate) << " & "
            << r.pageFaults << " & " << r.writeBacks << " & " << r.contextSwitches << " & "
            << r.threadSwitches << " & " << fmt(r.avgWait, 1) << " & " << fmt(r.jainFairness, 3)
            << " \\\\\n";
    }
    out << "\\bottomrule\n\\end{tabular}\n"
        << "% trace=" << safeTag << " TLB=" << f.tlbSize << " frames=" << f.frames
        << " page=" << f.pageSize << " accesses=" << f.accesses << "\n";
    return true;
}
