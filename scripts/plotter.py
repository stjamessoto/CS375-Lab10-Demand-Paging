#!/usr/bin/env python3
"""Automated timeline + comparison graphs for the CS375 Lab 10 simulator.

Reads the CSV files written by ./bin/vmsim and draws:

  timeline_<tag>_<policy>.png   CPU bursts per thread, coloured by TLB hit /
                                TLB miss / page fault, write-backs marked,
                                context + thread switches as vertical lines,
                                cumulative faults and TLB misses underneath
  policy_comparison.png         TLB hit %, fault %, switches, fairness per policy
  pattern_comparison.png        TLB hit % and fault % for each access pattern
  tlb_sweep.png                 TLB hit % vs TLB size, one line per pattern
  frames_sweep.png              page-fault % vs number of frames, per policy

If matplotlib is not installed it falls back to ASCII charts (printed and
saved as output/graphs/ascii_graphs.txt), so it always produces something.

Usage:
  python3 scripts/plotter.py                 # everything, tag "trace"
  python3 scripts/plotter.py --tag trace_localized
  python3 scripts/plotter.py --ascii         # force text output
"""
import argparse
import csv
import os
import sys

POLICIES = ["FCFS", "RR", "Priority", "MLFQ"]
PATTERNS = [("trace", "mixed (trace.txt)"), ("trace_sequential", "sequential"),
            ("trace_random", "random"), ("trace_localized", "localized")]

# Categorical palette (fixed order, validated for colour-vision deficiency).
SERIES = ["#2a78d6", "#eb6834", "#1baf7a", "#eda100"]
INK = "#0b0b0b"
INK_2 = "#52514e"
GRID = "#e4e3df"
SURFACE = "#fcfcfb"
WAIT = "#e9e8e4"


# --------------------------------------------------------------------- data

def read_csv(path):
    if not os.path.exists(path):
        return []
    with open(path, newline="") as f:
        return list(csv.DictReader(f))


def num(row, key):
    try:
        return float(row[key])
    except (KeyError, ValueError):
        return 0.0


def events_path(out, tag, policy):
    return os.path.join(out, "events", f"{tag}_{policy.lower()}.csv")


# --------------------------------------------------------------- matplotlib

def setup_mpl():
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    plt.rcParams.update({
        "figure.facecolor": SURFACE, "axes.facecolor": SURFACE, "savefig.facecolor": SURFACE,
        "axes.edgecolor": GRID, "axes.labelcolor": INK_2, "axes.titlecolor": INK,
        "axes.titleweight": "bold", "axes.titlesize": 11, "axes.titlelocation": "left",
        "xtick.color": INK_2, "ytick.color": INK_2, "text.color": INK,
        "axes.grid": True, "grid.color": GRID, "grid.linewidth": 0.6,
        "axes.spines.top": False, "axes.spines.right": False,
        "font.size": 9, "legend.frameon": False,
    })
    return plt


def plot_timeline(plt, ev, tag, policy, out_png):
    from matplotlib.patches import Patch
    from matplotlib.lines import Line2D

    threads = []
    for e in ev:
        k = (int(e["pid"]), int(e["tid"]))
        if k not in threads:
            threads.append(k)
    threads.sort()
    row = {k: i for i, k in enumerate(threads)}
    finish = {}
    for e in ev:
        finish[(int(e["pid"]), int(e["tid"]))] = int(e["time"]) + 1
    n = len(ev)

    fig, (ax, ax2) = plt.subplots(2, 1, figsize=(12, 1.2 + 0.42 * len(threads) + 2.2),
                                  gridspec_kw={"height_ratios": [len(threads) + 1, 3]},
                                  sharex=True)

    # waiting background bars
    for k, i in row.items():
        ax.barh(i, finish[k], left=0, height=0.62, color=WAIT, linewidth=0)

    cls_color = {"hit": SERIES[0], "miss": SERIES[1], "fault": SERIES[2]}
    for cls in ("hit", "miss", "fault"):
        xs, ys = [], []
        for e in ev:
            c = "fault" if e["page_fault"] == "1" else ("hit" if e["tlb"] == "hit" else "miss")
            if c == cls:
                xs.append(int(e["time"]))
                ys.append(row[(int(e["pid"]), int(e["tid"]))])
        ax.barh(ys, [1] * len(xs), left=xs, height=0.62, color=cls_color[cls], linewidth=0)

    wb_x = [int(e["time"]) + 0.5 for e in ev if e["write_back"] == "1"]
    wb_y = [row[(int(e["pid"]), int(e["tid"]))] + 0.48 for e in ev if e["write_back"] == "1"]
    ax.scatter(wb_x, wb_y, marker="v", s=14, color=INK, zorder=3, linewidths=0)

    for e in ev:
        if e["switch"] == "context":
            ax.axvline(int(e["time"]), color=INK_2, linewidth=0.7, alpha=0.55, zorder=0)
        elif e["switch"] == "thread":
            ax.axvline(int(e["time"]), color=INK_2, linewidth=0.7, linestyle=":", alpha=0.6, zorder=0)

    ax.set_yticks(range(len(threads)))
    ax.set_yticklabels([f"P{p}.T{t}" for p, t in threads])
    ax.invert_yaxis()
    ax.grid(axis="y", visible=False)
    ctx = sum(1 for e in ev if e["switch"] == "context")
    thr = sum(1 for e in ev if e["switch"] == "thread")
    ax.set_title(f"{policy} - CPU bursts per thread ({tag}, {n} ticks, "
                 f"{ctx} context switches, {thr} thread switches)")
    ax.legend(handles=[Patch(color=SERIES[0], label="TLB hit"),
                       Patch(color=SERIES[1], label="TLB miss (page-table hit)"),
                       Patch(color=SERIES[2], label="page fault"),
                       Patch(color=WAIT, label="ready / waiting"),
                       Line2D([], [], marker="v", color=INK, linestyle="", label="dirty write-back"),
                       Line2D([], [], color=INK_2, linewidth=0.8, label="context switch"),
                       Line2D([], [], color=INK_2, linewidth=0.8, linestyle=":", label="thread switch")],
              loc="upper center", bbox_to_anchor=(0.5, -0.02), ncol=7, fontsize=8)

    # cumulative counters (same unit -> one axis)
    t = list(range(1, n + 1))
    cf, cm, f_, m_ = [], [], 0, 0
    for e in ev:
        f_ += e["page_fault"] == "1"
        m_ += e["tlb"] == "miss"
        cf.append(f_)
        cm.append(m_)
    ax2.plot(t, cm, color=SERIES[1], linewidth=2)
    ax2.plot(t, cf, color=SERIES[2], linewidth=2)
    ax2.annotate(f"TLB misses {m_}", (n, m_), xytext=(4, 0), textcoords="offset points",
                 va="center", color=INK, fontsize=8)
    ax2.annotate(f"page faults {f_}", (n, f_), xytext=(4, 0), textcoords="offset points",
                 va="center", color=INK, fontsize=8)
    ax2.set_ylabel("cumulative count")
    ax2.set_xlabel("time (ticks = memory accesses)")
    ax2.set_xlim(0, n * 1.08)
    ax2.set_title("Cumulative TLB misses and page faults", fontsize=10)

    fig.tight_layout()
    fig.savefig(out_png, dpi=150)
    plt.close(fig)


def bar_panel(ax, labels, values, title, fmt="{:.1f}", color=SERIES[0]):
    bars = ax.bar(labels, values, color=color, width=0.6, linewidth=0)
    for b, v in zip(bars, values):
        ax.annotate(fmt.format(v), (b.get_x() + b.get_width() / 2, b.get_height()),
                    xytext=(0, 3), textcoords="offset points", ha="center", fontsize=8, color=INK)
    ax.set_title(title, fontsize=10)
    ax.grid(axis="x", visible=False)
    ax.margins(y=0.18)


def plot_policy_comparison(plt, rows, tag, out_png):
    rows = [r for r in rows if r["policy"] in POLICIES]
    rows.sort(key=lambda r: POLICIES.index(r["policy"]))
    labels = [r["policy"] for r in rows]
    fig, axes = plt.subplots(2, 3, figsize=(12, 6))
    panels = [("tlb_hit_rate", "TLB hit rate (%)", "{:.1f}"),
              ("page_fault_rate", "Page fault rate (%)", "{:.1f}"),
              ("context_switches", "Context switches", "{:.0f}"),
              ("thread_switches", "Thread switches", "{:.0f}"),
              ("avg_wait", "Average wait (ticks)", "{:.1f}"),
              ("jain_fairness", "Fairness (Jain's index, 1 = fair)", "{:.3f}")]
    for ax, (key, title, f) in zip(axes.flat, panels):
        bar_panel(ax, labels, [num(r, key) for r in rows], title, f)
    fig.suptitle(f"Scheduling policy comparison - {tag}", x=0.01, ha="left", fontweight="bold")
    fig.tight_layout()
    fig.savefig(out_png, dpi=150)
    plt.close(fig)


def plot_pattern_comparison(plt, rows, out_png):
    import numpy as np
    present = [(t, name) for t, name in PATTERNS if any(r["tag"] == t for r in rows)]
    if not present:
        return False
    fig, axes = plt.subplots(1, 2, figsize=(12, 4.2))
    x = np.arange(len(present))
    w = 0.2
    for ax, key, title in ((axes[0], "tlb_hit_rate", "TLB hit rate (%) by access pattern"),
                           (axes[1], "page_fault_rate", "Page fault rate (%) by access pattern")):
        for i, pol in enumerate(POLICIES):
            vals = []
            for t, _ in present:
                m = [r for r in rows if r["tag"] == t and r["policy"] == pol]
                vals.append(num(m[0], key) if m else 0)
            ax.bar(x + (i - 1.5) * w, vals, w * 0.92, color=SERIES[i], label=pol, linewidth=0)
        ax.set_xticks(x)
        ax.set_xticklabels([name for _, name in present])
        ax.set_ylim(0, 105)
        ax.set_title(title, fontsize=10)
        ax.grid(axis="x", visible=False)
    axes[0].legend(ncol=4, loc="upper left", fontsize=8)
    fig.tight_layout()
    fig.savefig(out_png, dpi=150)
    plt.close(fig)
    return True


def plot_tlb_sweep(plt, rows, out_png):
    if not rows:
        return False
    fig, axes = plt.subplots(1, 4, figsize=(13, 3.8), sharey=True)
    for ax, pol in zip(axes, POLICIES):
        for i, (t, name) in enumerate(PATTERNS):
            pts = sorted((num(r, "tlb_size"), num(r, "tlb_hit_rate"))
                         for r in rows if r["policy"] == pol and r["tag"].startswith(t + "_tlb"))
            if not pts:
                continue
            xs, ys = zip(*pts)
            ax.plot(xs, ys, color=SERIES[i], linewidth=2, marker="o", markersize=4, label=name)
        ax.set_xscale("log", base=2)
        ax.set_xticks([1, 2, 4, 8, 16, 32])
        ax.set_xticklabels(["1", "2", "4", "8", "16", "32"])
        ax.set_title(pol, fontsize=10)
        ax.set_xlabel("TLB entries")
        ax.set_ylim(0, 100)
    axes[0].set_ylabel("TLB hit rate (%)")
    handles, labels = axes[0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="upper right", ncol=4, fontsize=8)
    fig.suptitle("TLB hit rate vs TLB size", x=0.01, ha="left", fontweight="bold")
    fig.tight_layout(rect=(0, 0, 1, 0.92))
    fig.savefig(out_png, dpi=150)
    plt.close(fig)
    return True


def plot_frames_sweep(plt, rows, out_png):
    if not rows:
        return False
    fig, ax = plt.subplots(figsize=(7, 4))
    for i, pol in enumerate(POLICIES):
        pts = sorted((num(r, "frames"), num(r, "page_fault_rate")) for r in rows if r["policy"] == pol)
        if not pts:
            continue
        xs, ys = zip(*pts)
        ax.plot(xs, ys, color=SERIES[i], linewidth=2, marker="o", markersize=4, label=pol)
    ax.set_xlabel("physical frames")
    ax.set_ylabel("page fault rate (%)")
    ax.set_title("Page fault rate vs number of frames (trace.txt, TLB 8)")
    ax.legend(fontsize=8)
    fig.tight_layout()
    fig.savefig(out_png, dpi=150)
    plt.close(fig)
    return True


# -------------------------------------------------------------------- ASCII

def ascii_bars(title, items, width=40, fmt="{:.2f}"):
    lines = [title, "-" * len(title)]
    top = max([v for _, v in items] + [1e-9])
    for label, v in items:
        n = int(round(width * v / top)) if top > 0 else 0
        lines.append(f"  {label:<22} {'#' * n:<{width}} {fmt.format(v)}")
    return lines + [""]


def ascii_timeline(ev, policy, buckets=60):
    """Condensed timeline: each column is a bucket of ticks."""
    n = len(ev)
    if n == 0:
        return []
    size = max(1, -(-n // buckets))
    threads = sorted({(int(e["pid"]), int(e["tid"])) for e in ev})
    lines = [f"{policy} timeline ({n} ticks, 1 column = {size} ticks): "
             "'F' fault in bucket, 'm' TLB miss, '=' only hits, ' ' not running",
             "-" * 70]
    for th in threads:
        row = ""
        for b in range(0, n, size):
            chunk = [e for e in ev[b:b + size] if (int(e["pid"]), int(e["tid"])) == th]
            if not chunk:
                row += " "
            elif any(e["page_fault"] == "1" for e in chunk):
                row += "F"
            elif any(e["tlb"] == "miss" for e in chunk):
                row += "m"
            else:
                row += "="
        lines.append(f"  P{th[0]}.T{th[1]:<4} |{row}|")
    sw = ""
    for b in range(0, n, size):
        chunk = ev[b:b + size]
        sw += "C" if any(e["switch"] == "context" for e in chunk) else (
            "t" if any(e["switch"] == "thread" for e in chunk) else " ")
    lines.append(f"  {'Switch':<8} |{sw}|")
    return lines + [""]


def run_ascii(out, tag, gdir, have_png=False):
    out_lines = ["CS375 Lab 10 - ASCII graphs (text version of the PNG graphs)" if have_png else "CS375 Lab 10 - ASCII graphs (install matplotlib for PNG graphs)", ""]
    for pol in POLICIES:
        ev = read_csv(events_path(out, tag, pol))
        out_lines += ascii_timeline(ev, pol)
    rows = read_csv(os.path.join(out, "summary", f"{tag}.csv"))
    if rows:
        for key, title, f in (("tlb_hit_rate", "TLB hit rate (%)", "{:.2f}"),
                              ("page_fault_rate", "Page fault rate (%)", "{:.2f}"),
                              ("context_switches", "Context switches", "{:.0f}"),
                              ("thread_switches", "Thread switches", "{:.0f}"),
                              ("jain_fairness", "Fairness (Jain)", "{:.3f}")):
            out_lines += ascii_bars(f"{title} - {tag}", [(r["policy"], num(r, key)) for r in rows], fmt=f)
    pat = read_csv(os.path.join(out, "summary", "all_patterns.csv"))
    if pat:
        for key, title in (("tlb_hit_rate", "TLB hit rate (%) by pattern"),
                           ("page_fault_rate", "Page fault rate (%) by pattern")):
            items = [(f"{r['tag'].replace('trace_', '')}/{r['policy']}", num(r, key)) for r in pat]
            out_lines += ascii_bars(title, items)
    text = "\n".join(out_lines)
    print(text)
    path = os.path.join(gdir, "ascii_graphs.txt")
    with open(path, "w") as f:
        f.write(text + "\n")
    print(f"Saved {path}")


# --------------------------------------------------------------------- main

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", default="output", help="simulator output folder (default output)")
    ap.add_argument("--tag", default="trace", help="which run to draw timelines for (default trace)")
    ap.add_argument("--ascii", action="store_true", help="force ASCII output")
    args = ap.parse_args()

    gdir = os.path.join(args.out, "graphs")
    os.makedirs(gdir, exist_ok=True)

    if not any(os.path.exists(events_path(args.out, args.tag, p)) for p in POLICIES):
        sys.exit(f"No event files for tag '{args.tag}' in {args.out}/events. "
                 f"Run ./bin/vmsim first (or make experiments).")

    plt = None
    if not args.ascii:
        try:
            plt = setup_mpl()
        except ImportError:
            print("matplotlib not found -> ASCII graphs. For PNG graphs install it with:\n"
                  "    sudo apt install python3-matplotlib      (or: pip install matplotlib)\n")
    if plt is None:
        run_ascii(args.out, args.tag, gdir)
        return

    made = []
    for pol in POLICIES:
        ev = read_csv(events_path(args.out, args.tag, pol))
        if ev:
            p = os.path.join(gdir, f"timeline_{args.tag}_{pol.lower()}.png")
            plot_timeline(plt, ev, args.tag, pol, p)
            made.append(p)
    rows = read_csv(os.path.join(args.out, "summary", f"{args.tag}.csv"))
    if rows:
        p = os.path.join(gdir, "policy_comparison.png")
        plot_policy_comparison(plt, rows, args.tag, p)
        made.append(p)
    p = os.path.join(gdir, "pattern_comparison.png")
    if plot_pattern_comparison(plt, read_csv(os.path.join(args.out, "summary", "all_patterns.csv")), p):
        made.append(p)
    p = os.path.join(gdir, "tlb_sweep.png")
    if plot_tlb_sweep(plt, read_csv(os.path.join(args.out, "summary", "tlb_sweep.csv")), p):
        made.append(p)
    p = os.path.join(gdir, "frames_sweep.png")
    if plot_frames_sweep(plt, read_csv(os.path.join(args.out, "summary", "frames_sweep.csv")), p):
        made.append(p)

    run_ascii(args.out, args.tag, gdir, have_png=True)   # the text version is always useful for screenshots
    print("\nPNG graphs:")
    for m in made:
        print("  " + m)
    if not os.path.exists(os.path.join(args.out, "summary", "tlb_sweep.csv")):
        print("(Run 'make experiments' to also get the pattern / TLB / frame sweep graphs.)")


if __name__ == "__main__":
    main()
