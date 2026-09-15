#!/usr/bin/env python3
"""
plot_mlfq.py - turns schedulertest's "<tick> <pid> <priority>" log lines
into the timeline scatter plot required by the mini-project report
(section 2.3.2): X = tick, Y = MLFQ queue (0-3), colored by pid.

Usage:
    schedulertest is run inside qemu; capture its console output to a
    file (e.g. mlfq_raw.log), then:

        python3 plot_mlfq.py mlfq_raw.log out.png "yourusername"

    where "yourusername" is the part of your IIIT email before the @,
    used to watermark the plot per the assignment's requirement.
"""
import re
import sys
import matplotlib.pyplot as plt

def parse(path):
    rows = []
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.strip()
            m = re.match(r"^(\d+)\s+(\d+)\s+(-?\d+)$", line)
            if not m:
                continue
            tick, pid, prio = int(m.group(1)), int(m.group(2)), int(m.group(3))
            if 0 <= prio <= 3:
                rows.append((tick, pid, prio))
    return rows

def main():
    if len(sys.argv) < 2:
        print("usage: plot_mlfq.py <log_file> [out.png] [watermark]")
        sys.exit(1)

    log_path = sys.argv[1]
    out_path = sys.argv[2] if len(sys.argv) > 2 else "mlfq_timeline.png"
    watermark = sys.argv[3] if len(sys.argv) > 3 else "username"

    rows = parse(log_path)
    if not rows:
        print("no matching '<tick> <pid> <priority>' lines found")
        sys.exit(1)

    pids = sorted(set(r[1] for r in rows))
    colors = plt.cm.tab10.colors

    fig, ax = plt.subplots(figsize=(11, 5))
    for i, pid in enumerate(pids):
        xs = [t for t, p, q in rows if p == pid]
        ys = [q for t, p, q in rows if p == pid]
        ax.scatter(xs, ys, label=f"pid {pid}", color=colors[i % len(colors)], s=18)

    # mark every 48-tick boost boundary
    max_tick = max(r[0] for r in rows)
    boost = 48
    b = boost
    first = True
    while b <= max_tick:
        ax.axvline(b, color="gray", linestyle="--", linewidth=0.8,
                   label="priority boost" if first else None)
        first = False
        b += boost

    ax.set_xlabel("Ticks elapsed since scheduler start")
    ax.set_ylabel("MLFQ queue (0 = highest, 3 = lowest)")
    ax.set_yticks([0, 1, 2, 3])
    ax.set_title("MLFQ queue timeline (schedulertest)")
    ax.legend(loc="upper right", fontsize=8)
    ax.grid(True, alpha=0.3)

    fig.text(0.5, 0.5, watermark, fontsize=40, color="gray",
              alpha=0.15, ha="center", va="center", rotation=30,
              transform=ax.transAxes)

    fig.tight_layout()
    fig.savefig(out_path, dpi=150)
    print(f"wrote {out_path} ({len(rows)} points, {len(pids)} pids)")

if __name__ == "__main__":
    main()
