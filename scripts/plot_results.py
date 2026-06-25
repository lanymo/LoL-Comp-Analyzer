"""
plot_results.py — Generate experiment graphs from eval/results/*.csv.

Reads the CSVs produced by eval/benchmark.sh:
  - thread_scaling.csv  (omp_threads,clients,r,ok,errors,throughput_rps,mean_ms,p50_ms,p95_ms,p99_ms)
  - concurrency.csv     (clients,r,ok,errors,throughput_rps,mean_ms,p50_ms,p95_ms,p99_ms)
  - intensity.csv       (clients,r,ok,errors,throughput_rps,mean_ms,p50_ms,p95_ms,p99_ms)

Produces, per the report's required graphs:
  - thread_scaling.png       speedup vs OpenMP threads
  - concurrency_latency.png  latency vs concurrent clients
  - concurrency_throughput.png throughput vs concurrent clients
  - intensity.png            latency vs bootstrap R (compute-intensity curve)

Usage:
  python scripts/plot_results.py
  python scripts/plot_results.py --results-dir eval/results --out-dir eval/results
"""

import argparse
import csv
from pathlib import Path

import matplotlib.pyplot as plt

# Repo root = parent of the scripts/ directory holding this file.
REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_RESULTS_DIR = REPO_ROOT / "eval" / "results"


def read_csv(path: Path) -> list[dict[str, float]]:
    """Read a benchmark CSV into a list of {column: float} rows.

    Returns [] (with a warning) if the file is missing or has no data rows,
    so a not-yet-run experiment doesn't crash the whole plotting pass.
    """
    if not path.exists():
        print(f"[plot] skip: {path} not found")
        return []
    with path.open(newline="") as f:
        rows = [{k: float(v) for k, v in row.items()} for row in csv.DictReader(f)]
    if not rows:
        print(f"[plot] skip: {path} has no data rows")
    return rows


def plot_thread_scaling(results_dir: Path, out_dir: Path) -> None:
    rows = read_csv(results_dir / "thread_scaling.csv")
    if not rows:
        return
    rows.sort(key=lambda r: r["omp_threads"])
    threads = [r["omp_threads"] for r in rows]

    # Speedup is measured against the single-thread latency baseline.
    baseline = next((r["mean_ms"] for r in rows if r["omp_threads"] == 1), rows[0]["mean_ms"])
    speedups = [baseline / r["mean_ms"] for r in rows]

    plt.figure()
    plt.plot(threads, speedups, marker="o", label="measured")
    plt.plot(threads, threads, linestyle="--", color="gray", label="ideal (linear)")
    plt.xlabel("OpenMP Threads")
    plt.ylabel("Speedup")
    plt.title("Thread Scaling")
    plt.legend()
    plt.grid(True, alpha=0.3)
    out = out_dir / "thread_scaling.png"
    plt.savefig(out, dpi=150, bbox_inches="tight")
    print(f"[plot] -> {out}")


def plot_concurrency(results_dir: Path, out_dir: Path) -> None:
    rows = read_csv(results_dir / "concurrency.csv")
    if not rows:
        return
    rows.sort(key=lambda r: r["clients"])
    clients = [r["clients"] for r in rows]

    plt.figure()
    plt.plot(clients, [r["mean_ms"] for r in rows], marker="s", label="mean")
    plt.plot(clients, [r["p95_ms"] for r in rows], marker="^", linestyle="--", label="p95")
    plt.xlabel("Concurrent Clients")
    plt.ylabel("Latency (ms)")
    plt.title("Concurrency Scaling — Latency")
    plt.legend()
    plt.grid(True, alpha=0.3)
    out = out_dir / "concurrency_latency.png"
    plt.savefig(out, dpi=150, bbox_inches="tight")
    print(f"[plot] -> {out}")

    plt.figure()
    plt.plot(clients, [r["throughput_rps"] for r in rows], marker="o", color="tab:green")
    plt.xlabel("Concurrent Clients")
    plt.ylabel("Throughput (req/s)")
    plt.ylim(bottom=0)  # 0부터 시작해야 포화 구간의 미세 변동이 과장되지 않음
    plt.title("Concurrency Scaling — Throughput")
    plt.grid(True, alpha=0.3)
    out = out_dir / "concurrency_throughput.png"
    plt.savefig(out, dpi=150, bbox_inches="tight")
    print(f"[plot] -> {out}")


def plot_intensity(results_dir: Path, out_dir: Path) -> None:
    rows = read_csv(results_dir / "intensity.csv")
    if not rows:
        return
    rows.sort(key=lambda r: r["r"])
    r_vals = [r["r"] for r in rows]

    plt.figure()
    plt.plot(r_vals, [r["mean_ms"] for r in rows], marker="o")
    plt.xlabel("Bootstrap R (resamples)")
    plt.ylabel("Mean Latency (ms)")
    plt.title("Compute Intensity — Latency vs Bootstrap R")

    # R=0(통신+직렬화 바닥값)은 이 스케일에선 안 보이므로 값을 주석으로 표기
    base = next((r for r in rows if r["r"] == 0), None)
    if base is not None:
        plt.annotate(f"R=0: {base['mean_ms']:.2f} ms\n(communication floor)",
                     xy=(base["r"], base["mean_ms"]),
                     xytext=(0.05, 0.55), textcoords="axes fraction",
                     arrowprops=dict(arrowstyle="->", color="gray"))
    plt.grid(True, alpha=0.3)
    out = out_dir / "intensity.png"
    plt.savefig(out, dpi=150, bbox_inches="tight")
    print(f"[plot] -> {out}")


def main() -> None:
    parser = argparse.ArgumentParser(description="Plot benchmark results from eval CSVs.")
    parser.add_argument("--results-dir", type=Path, default=DEFAULT_RESULTS_DIR,
                        help="directory holding the benchmark CSVs")
    parser.add_argument("--out-dir", type=Path, default=None,
                        help="directory to write PNGs into (default: results-dir)")
    args = parser.parse_args()

    results_dir = args.results_dir
    out_dir = args.out_dir or results_dir
    out_dir.mkdir(parents=True, exist_ok=True)

    plot_thread_scaling(results_dir, out_dir)
    plot_concurrency(results_dir, out_dir)
    plot_intensity(results_dir, out_dir)


if __name__ == "__main__":
    main()
