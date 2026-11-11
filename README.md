# CS375 Lab 10 – Demand Paging, Advanced Clock, TLB & MLFQ Simulator

A C++17 simulator of a demand-paged virtual memory system working together with a CPU/thread
scheduler. Every memory reference in a trace goes through:

**LRU TLB → two-level page table → page fault → advanced Clock (R + M bits)**

The trace is replayed under **FCFS, Round Robin, Priority and MLFQ**. The simulator logs every
access and produces ASCII and matplotlib timeline graphs, summary tables, and a LaTeX report.

---

## Quick start

```bash
make                 # 1. build  -> bin/vmsim
./bin/vmsim          # 2. run all 4 policies on input/trace.txt
make experiments     # 3. run every pattern / policy / TLB size / frame count
make plots           # 4. draw graphs        -> output/graphs/
make report          # 5. build the report   -> report/report.pdf
make submission      # 6. package everything -> submission/
```

---

## Folder structure (what to submit is marked ✅)

```
CS375-Lab10-Demand-Paging/
├── src/                     ✅ SOURCE CODE TO SUBMIT (all .cpp and .h files)
│   ├── main.cpp                 driver
│   ├── Process.h/.cpp           process + threads (per-thread accesses, bursts, priorities)
│   ├── CPU.h/.cpp               CPU cycles, context vs thread switches
│   ├── Scheduler.h/.cpp         FCFS, RR (q=4), Priority, MLFQ (3 queues + aging)
│   ├── TLB.h/.cpp               configurable-size TLB, LRU eviction
│   ├── TwoLevelPageTable.h/.cpp two-level page table, on-demand inner tables
│   ├── Clock.h/.cpp             advanced Clock with reference + modified bits
│   ├── MMU.h/.cpp               translation path (TLB -> walk -> fault -> Clock)
│   ├── TraceLoader.h/.cpp       trace file parser
│   ├── Config.h/.cpp            command-line + config-file options
│   ├── Logger.h/.cpp            auto-generated log file
│   └── Report.h/.cpp            metrics, summary tables, CSV/LaTeX export, ASCII timeline
├── input/                   ✅ MODIFIED TRACES TO SUBMIT
│   ├── trace.txt                modified trace: original sample + sequential/random/localized
│   ├── trace_sequential.txt     same 8 threads, sequential pattern only
│   ├── trace_random.txt         same 8 threads, random pattern only
│   ├── trace_localized.txt      same 8 threads, localized pattern only
│   └── clock_demo.txt           7-line demo of the Clock clean-before-dirty rule
├── output/                  (auto-generated – created by the simulator)
│   ├── logs/                ✅ OUTPUT LOG FILES  <trace>_<policy>.log
│   ├── events/                  per-tick CSVs used by plotter.py
│   ├── summary/                 summary CSVs + LaTeX tables used by the report
│   ├── graphs/                  PNG timelines/comparisons + ascii_graphs.txt
│   └── sweep/                   TLB/frame sweep runs (not committed, large)
├── report/
│   ├── report.tex               the written report (LaTeX)
│   ├── report.pdf           ✅ WRITTEN REPORT TO SUBMIT (after `make report`)
│   └── screenshots/             ⬅ PUT YOUR SCREENSHOTS HERE (01_build.png, ...)
├── scripts/
│   ├── plotter.py               automated timeline + comparison graphs
│   ├── gen_traces.py            regenerates the traces in input/ (fixed seed)
│   ├── run_experiments.sh       runs every experiment for the report
│   └── make_submission.sh       builds submission/
├── config/sim.conf              example config file (TLB size, frames, policy, ...)
├── Makefile
└── submission/              ✅ EVERYTHING TO HAND IN (rebuild with `make submission`)
    ├── src/                     all .cpp / .h files
    ├── input/trace.txt          modified trace with varied access patterns
    ├── output.log               auto-generated log (all 4 policies + summary table)
    ├── report.pdf               written report
    ├── Makefile                 build + run (`make`, then `./vmsim`)
    └── README.txt               how to build and run
```

### Submission checklist (from the handout)

| Handout requirement | Where it is |
|---|---|
| All `.cpp` and `.h` files | `src/` |
| Written report: summary table, timeline graphs, two-page analysis | `report/report.pdf` (from `report/report.tex`) |
| Output log file (auto-generated) | `output/trace_output.log` (all policies + summary; per-policy logs in `output/logs/`) |
| Modified `input/trace.txt` with varied access patterns | `input/trace.txt` |

**`submission/` already holds all of these.** It is a copy, so after you add screenshots or change
anything, refresh it:

```bash
make report && make submission
```

This rebuilds `submission/` (only the files above, plus a small Makefile and README.txt so it
builds and runs on its own). It also reports how many of the 13
screenshots made it into `report.pdf`. Upload the contents of `submission/`.

---

## Requirements

| Tool | Needed for | Install (Ubuntu / WSL) |
|---|---|---|
| `g++` (C++17) + `make` | building the simulator | `sudo apt install build-essential` |
| `python3` | traces, plots | usually preinstalled |
| `matplotlib` *(optional)* | PNG graphs. Without it, `plotter.py` prints ASCII graphs | `sudo apt install python3-matplotlib` |
| `pdflatex` | building the report | `sudo apt install texlive-latex-extra` |

Without `make`, build by hand with: `g++ -std=c++17 -O2 -o bin/vmsim src/*.cpp`

---

## Using the simulator

```bash
./bin/vmsim                                   # all policies, input/trace.txt, TLB 8, 16 frames
./bin/vmsim --policy mlfq --verbose           # one policy, print the full per-tick log
./bin/vmsim --tlb 4 --frames 8                # change TLB size and memory size
./bin/vmsim --trace input/trace_random.txt    # another trace
./bin/vmsim --config config/sim.conf          # settings from a file (flags still override)
./bin/vmsim --help                            # every option
```

| Option | Meaning | Default |
|---|---|---|
| `--trace FILE` | trace to simulate | `input/trace.txt` |
| `--config FILE` | `key = value` settings file | – |
| `--policy LIST` | `fcfs,rr,priority,mlfq` or `all` | `all` |
| `--tlb N` | TLB entries (LRU); `0` disables the TLB | 8 |
| `--frames N` | physical frames (Clock) | 16 |
| `--page-size N` | bytes per page (power of 2) | 256 |
| `--address-bits N` | virtual address width | 16 |
| `--tlb-flush` | flush the TLB on every context switch (untagged TLB) | off |
| `--quantum N` | RR/Priority quantum, MLFQ Q0 quantum (Q1 = 2N, Q2 = 4N) | 4 |
| `--aging N` | MLFQ: promote after waiting N ticks | 30 |
| `--out DIR` / `--tag NAME` | output folder / file-name prefix | `output` / trace name |
| `--summary FILE` | append one CSV row per policy to FILE | – |
| `--verbose` / `--quiet` | full log on screen / summary only | – |
| `--no-timeline`, `--timeline-ticks N` | hide the ASCII timeline / ticks shown (0 = all) | 200 ticks |

### Trace format (`input/*.txt`)

```
# comment
<pid> <thread_id> <address> <R|W>     e.g.  1 2 0x00A1 W
PRIORITY <pid> <priority>             optional, lower number = higher priority (default 5)
THREAD <pid> <thread_id> <priority>   optional, per-thread priority (default: process priority)
```

Run `python3 scripts/gen_traces.py` (or `make traces`) to regenerate the traces. The seed is fixed,
so results are always the same.

---

## How it works (rubric map)

| Rubric item | Implementation |
|---|---|
| **Two-level page table + page faults** (15) | 16-bit VA = 4 outer + 4 inner + 8 offset bits. `firstLevel` maps (pid, outer) to an inner-table id; `secondLevel` maps (table id, inner) to a frame. Inner tables are allocated on the first fault in their range. On eviction the PTE is invalidated *and* the TLB entry is removed. |
| **Advanced Clock with modified bits** (15) | Pass A looks for (R=0, M=0) without changing bits. Pass B looks for (R=0, M=1) and clears R bits as it goes. Repeat. Clean pages are evicted first, and dirty victims count as disk write-backs. See `input/clock_demo.txt`. |
| **Configurable TLB with LRU** (15) | `--tlb N` or `tlb_size` in `config/sim.conf`; PID-tagged entries with a `lastUsed` timestamp; optional flush on context switch. |
| **Scheduler with MLFQ + threads** (15) | Schedules threads (pid, tid). FCFS; RR q=4; Priority (round robin among ties); MLFQ with 3 queues (q = 4/8/16), demotion after a full quantum, and aging promotion after 30 ticks. CPU counts *context* switches (different process) and *thread* switches (same process). |
| **Automated timeline graphs** (10) | ASCII timeline in the terminal and log; `scripts/plotter.py` draws a PNG Gantt chart per policy (hits/misses/faults/write-backs/switches) plus comparison graphs. |
| **Log output + performance summary** (5) | `output/logs/*.log` (every access, dispatch, preemption, demotion, promotion, eviction) plus the final summary table. |
| **Modified trace + pattern analysis** (5) | `input/trace.txt` with sequential, random and localized sections, plus one trace per pattern. |
| **Detailed analysis report** (20) | `report/report.tex` → `report/report.pdf` |

### Headline results (`input/trace.txt`, TLB 8, 16 frames)

| Policy | TLB hit % | Fault % | Write-backs | Ctx sw | Thr sw | Avg wait | Fairness |
|---|---|---|---|---|---|---|---|
| FCFS | 63.78 | 35.60 | 52 | 5 | 2 | 141.2 | 0.576 |
| RR | 39.01 | 48.30 | 60 | 67 | 14 | 184.0 | 0.943 |
| Priority | 62.23 | 34.98 | 51 | 42 | 18 | 145.8 | 0.715 |
| MLFQ | 46.44 | 48.61 | 61 | 41 | 7 | 175.0 | 0.947 |

FCFS and Priority keep each thread's locality, so they get the best TLB and fault rates but are
unfair. RR is fair but has the worst memory behaviour. MLFQ is about as fair as RR with 39% fewer
context switches. Hit rate stops improving past 16 TLB entries because the TLB cannot map more
pages than the 16 frames hold.

---

## Taking the screenshots for the report

Save each screenshot in **`report/screenshots/`** with the exact name below (`.png` or `.jpg`).
Then run `make report`, and the report places each one in its section automatically. Missing
screenshots appear as labelled boxes, so you can build the report at any point.

First run the pipeline once so every file exists: `make && make experiments && make plots`.
**Maximize the terminal window**, because the log lines are wide.

| File name | Command(s) to run | What to capture |
|---|---|---|
| `01_build.png` | `make clean && make` | compile lines + link of `bin/vmsim` |
| `02_help.png` | `./bin/vmsim --help` | the whole usage text |
| `03_trace.png` | `head -n 45 input/trace.txt` | header, priorities, original sample, start of sequential section |
| `04_page_table.png` | `grep -m 15 -E "PAGE FAULT\|page table hit" output/logs/trace_fcfs.log` then `grep -A5 "Two-level page table" output/logs/trace_fcfs.log` | outer/inner split, faults, and the page-table stats |
| `05_tlb.png` | `./bin/vmsim --tlb 2 --quiet --tag tlb2 \| head -11` then `./bin/vmsim --tlb 16 --quiet --tag tlb16 \| head -11` | both summary tables (TLB hit % changes, fault % does not) |
| `06_clock.png` | `./bin/vmsim --trace input/clock_demo.txt --frames 3 --policy fcfs --verbose --no-timeline \| grep -E "^\[t\|Advanced\|clean\|dirty\|frame "` | the evictions + Clock stats + frame table with R/M bits |
| `07_mlfq.png` | `grep -m 15 -E "DEMOTE\|PROMOTE" output/logs/trace_mlfq.log` then `grep -A11 "Per-thread" output/logs/trace_mlfq.log` | demotions, aging promotions, per-thread table |
| `08_threads.png` | `grep -m 16 -E "DISPATCH\|SWITCH" output/logs/trace_rr.log` then `grep -A11 "Per-thread" output/logs/trace_rr.log` | thread vs context switches + per-thread table |
| `09_ascii_timeline.png` | `sed -n '/ASCII timeline/,$p' output/logs/trace_rr.log \| head -27` | the text timeline |
| `10_summary.png` | `./bin/vmsim --quiet` | the performance summary table |
| `11_patterns.png` | `make experiments` | the first summary tables it prints |
| `12_plotter.png` | `make plots \| tail -n 24` | the end of the output (the "PNG graphs:" list) |
| `13_log_files.png` | `ls -lh output/logs output/trace_output.log` then `head -n 30 output/logs/trace_mlfq.log` | the log files and the start of one log |

> In the table above `\|` is just an escaped pipe for Markdown. In the terminal, type a normal `|`.

---

## Editing and building the report

1. Open `report/report.tex` and set your name and ID at the top (`\studentname`, `\studentid`).
2. Add the screenshots to `report/screenshots/`.
3. `make report` produces `report/report.pdf`.
4. `make submission` refreshes `submission/` (always run it last).

The summary tables and graphs in the report are read from `output/`, so re-running
`make experiments && make plots` before `make report` keeps them in sync with the code.

## Troubleshooting

| Problem | Fix |
|---|---|
| `cannot open trace file` | run from the project folder (where the `Makefile` is) |
| `plotter.py` prints ASCII instead of PNG | `sudo apt install python3-matplotlib` |
| `pdflatex: command not found` | `sudo apt install texlive-latex-extra`, or upload `report/` to Overleaf (also upload `output/summary/*_table.tex` and `output/graphs/*.png`, keeping the same relative paths) |
| Report shows "Graph ... not found" | run `make experiments && make plots` first |
| `make submission` says no logs / no report | run `make experiments` / `make report` first |
| `submission/` has an old report | it is a copy: run `make report && make submission` again |
