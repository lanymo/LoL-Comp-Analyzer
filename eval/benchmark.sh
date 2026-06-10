#!/usr/bin/env bash
#
# benchmark.sh — 성능 측정 스윕 드라이버
#
# 보고서 그래프 3종의 원자료(CSV)를 자동 생성한다:
#   1) thread_scaling.csv  — OpenMP 스레드 1·2·4·8 → speedup
#   2) concurrency.csv     — 동시 클라이언트 1·2·4·8·16 → latency / throughput
#   3) intensity.csv       — 부트스트랩 R 0·100·500·1000 → 연산 강도 곡선
#
# 핵심 제약: OpenMP 스레드 수(OMP_NUM_THREADS)는 "서버 기동 시점"에만 적용된다.
#            → thread scaling은 값마다 서버를 죽였다 다시 띄운다.
#            → concurrency / intensity는 서버 한 번 띄워두고 클라이언트만 바꾼다.
#
# 사용법:
#   make                      # 먼저 빌드
#   bash eval/benchmark.sh    # 전체 스윕 실행
#
# 환경변수로 튜닝 가능(무거우면 줄여서 빠르게):
#   REQUESTS=10 WARMUP=2 bash eval/benchmark.sh

set -euo pipefail

# ─── 설정 ─────────────────────────────────────────────────────────────────────
SERVER=./build/analyzer_server
BENCH=./build/analyzer_benchmark
TARGET=localhost:50051
OUTDIR=eval/results

REQUESTS=${REQUESTS:-20}   # 측정 요청 수(스레드당)
WARMUP=${WARMUP:-5}        # 버릴 워밍업 요청 수(스레드당)

# 벤치마크가 뱉는 CSV 한 줄의 컬럼 정의
CSV_HEADER="clients,r,ok,errors,throughput_rps,mean_ms,p50_ms,p95_ms,p99_ms"

mkdir -p "$OUTDIR"
SERVER_PID=""

# ─── 서버 기동/종료 헬퍼 ──────────────────────────────────────────────────────
# OMP_NUM_THREADS를 지정해 서버를 백그라운드로 띄우고, "listening" 로그가 뜰 때까지
# 기다린다. 서버는 기동 시 CSV 로드 + 인덱스 빌드를 하므로 즉시 준비되지 않는다.
start_server() {
    local omp=$1
    echo "[bench] starting server (OMP_NUM_THREADS=$omp) ..."
    OMP_NUM_THREADS="$omp" "$SERVER" > "$OUTDIR/server.log" 2>&1 &
    SERVER_PID=$!

    for _ in $(seq 1 120); do          # 최대 60초 대기 (0.5s × 120)
        if grep -q "Server listening" "$OUTDIR/server.log" 2>/dev/null; then
            echo "[bench] server ready (pid=$SERVER_PID)"
            return 0
        fi
        if ! kill -0 "$SERVER_PID" 2>/dev/null; then
            echo "[bench] !! server died during startup. log:"
            cat "$OUTDIR/server.log"
            exit 1
        fi
        sleep 0.5
    done
    echo "[bench] !! server did not become ready in time"
    exit 1
}

stop_server() {
    if [[ -n "$SERVER_PID" ]] && kill -0 "$SERVER_PID" 2>/dev/null; then
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
        echo "[bench] server stopped"
    fi
    SERVER_PID=""
}

# 스크립트가 어떤 이유로 끝나도(에러·Ctrl+C) 서버를 반드시 정리
trap stop_server EXIT

# 벤치마크 1회 실행 → CSV 한 줄을 stdout으로 반환
run_capture() {
    local clients=$1 r=$2
    "$BENCH" --target "$TARGET" \
             --clients "$clients" --requests "$REQUESTS" --warmup "$WARMUP" \
             --r "$r"
}

# ─── 실험 1: Thread scaling (speedup) ─────────────────────────────────────────
# 단일 클라이언트(요청 경쟁 없음)로 순수 OpenMP 가속만 측정.
# R은 무겁게(1000) 둬야 병렬 이득이 또렷하다.
# 출력에 omp_threads 컬럼을 앞에 덧붙인다(벤치마크는 서버 스레드 수를 모르므로).
thread_scaling() {
    local r=${1:-1000}
    local out="$OUTDIR/thread_scaling.csv"
    echo "omp_threads,$CSV_HEADER" > "$out"
    echo "[bench] === experiment 1: thread scaling (r=$r, clients=1) ==="
    for t in 1 2 4 8; do
        start_server "$t"
        local line
        line=$(run_capture 1 "$r")
        echo "$t,$line" >> "$out"
        echo "[bench]   omp=$t -> $line"
        stop_server
    done
    echo "[bench] -> $out"
}

# ─── 실험 2: Concurrency scaling (latency / throughput) ───────────────────────
# 서버를 전체 코어(OMP=nproc)로 한 번 띄우고 동시 클라이언트 수만 키운다.
# 한 요청이 이미 모든 코어를 쓰므로, 동시성↑ 시 경쟁이 드러난다(보고서 분석거리).
concurrency_scaling() {
    local omp=${1:-$(nproc)}
    local r=${2:-500}
    local out="$OUTDIR/concurrency.csv"
    echo "$CSV_HEADER" > "$out"
    echo "[bench] === experiment 2: concurrency scaling (omp=$omp, r=$r) ==="
    start_server "$omp"
    for c in 1 2 4 8 16; do
        local line
        line=$(run_capture "$c" "$r")
        echo "$line" >> "$out"
        echo "[bench]   clients=$c -> $line"
    done
    stop_server
    echo "[bench] -> $out"
}

# ─── 실험 3: Compute intensity (병목 이동) ────────────────────────────────────
# 단일 클라이언트로 R만 키운다. R=0이 통신 바닥값, R↑에서 CPU가 병목이 됨.
intensity_sweep() {
    local omp=${1:-$(nproc)}
    local clients=${2:-1}
    local out="$OUTDIR/intensity.csv"
    echo "$CSV_HEADER" > "$out"
    echo "[bench] === experiment 3: compute intensity (omp=$omp, clients=$clients) ==="
    start_server "$omp"
    for r in 0 100 500 1000; do
        local line
        line=$(run_capture "$clients" "$r")
        echo "$line" >> "$out"
        echo "[bench]   r=$r -> $line"
    done
    stop_server
    echo "[bench] -> $out"
}

# ─── main ─────────────────────────────────────────────────────────────────────
main() {
    [[ -x "$SERVER" ]] || { echo "[bench] $SERVER 없음 — 먼저 'make' 하세요"; exit 1; }
    [[ -x "$BENCH"  ]] || { echo "[bench] $BENCH 없음 — 먼저 'make' 하세요";  exit 1; }

    # 첫 인자로 돌릴 실험 선택(없으면 전체). thread|concurrency|intensity|all
    local which=${1:-all}
    echo "[bench] run='$which'  REQUESTS=$REQUESTS WARMUP=$WARMUP  (무거우면 'REQUESTS=10 WARMUP=2'로 조절)"
    local t0=$SECONDS

    case "$which" in
        thread)      thread_scaling      1000 ;;
        concurrency) concurrency_scaling ""   500 ;;
        intensity)   intensity_sweep     ""   1 ;;
        all)
            thread_scaling      1000
            concurrency_scaling ""   500
            intensity_sweep     ""   1
            ;;
        *)
            echo "[bench] 알 수 없는 실험: '$which'"
            echo "[bench] 사용법: bash eval/benchmark.sh [thread|concurrency|intensity|all]"
            exit 1
            ;;
    esac

    echo "[bench] done ('$which') in $((SECONDS - t0))s. results in $OUTDIR/"
}

main "$@"
