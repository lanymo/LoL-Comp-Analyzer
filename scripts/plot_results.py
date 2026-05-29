"""
plot_results.py — Generate experiment graphs (thread scaling, concurrency, bootstrap).
"""

import matplotlib.pyplot as plt


def plot_thread_scaling(threads: list[int], speedups: list[float]) -> None:
    plt.figure()
    plt.plot(threads, speedups, marker="o")
    plt.xlabel("OpenMP Threads")
    plt.ylabel("Speedup")
    plt.title("Thread Scaling")
    plt.savefig("thread_scaling.png", dpi=150)


def plot_concurrency(clients: list[int], latencies: list[float]) -> None:
    plt.figure()
    plt.plot(clients, latencies, marker="s")
    plt.xlabel("Concurrent Clients")
    plt.ylabel("Latency (ms)")
    plt.title("Concurrency Scaling")
    plt.savefig("concurrency_scaling.png", dpi=150)


if __name__ == "__main__":
    # TODO: load actual measurement CSVs
    plot_thread_scaling([1, 2, 4, 8], [1.0, 1.9, 3.6, 6.8])
    plot_concurrency([1, 2, 4, 8, 16], [5, 5.2, 5.8, 7.1, 12.3])
