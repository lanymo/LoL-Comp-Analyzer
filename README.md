# LoL 챔피언 조합 분석 서비스

**과제:** CSEG414/5414 Parallel & Distributed Computing

**스택:** C++17 + gRPC(protobuf) + OpenMP

**요약:** 클라이언트가 챔피언 조합을 질의하면, 서버가 51,490개 랭크 매치 통계에서
쌍(pair) 기반 승률과 부트스트랩 95% 신뢰구간을 계산해 응답하는 분석 서비스.

- **요청 수준 병렬성:** gRPC 워커 스레드가 동시 클라이언트 처리
- **연산 수준 병렬성:** 질의당 부트스트랩 리샘플링을 OpenMP로 분배

---

## 디렉터리 구조

```
lol-comp-analyzer/
├── README.md               # 빌드/실행/실험 재현 절차
├── Makefile                # 빌드 설정 (proto 생성 + server/client/benchmark)
├── proto/
│   └── analyzer.proto      # gRPC 서비스 정의 (protobuf)
├── server/
│   ├── main.cc             # gRPC 서버 진입점
│   ├── analyzer_service.cc # AnalyzeComposition / RecommendPick 핸들러
│   └── stats_engine.cc     # CSV 로드, 쌍 집계, 부트스트랩 (OpenMP 병렬화)
├── client/
│   ├── main.cc             # 단일 질의 CLI 클라이언트
│   └── benchmark.cc        # 동시 다중 클라이언트 벤치마크
├── eval/
│   ├── benchmark.sh        # 실험 4종 자동 스윕 드라이버
│   └── results/            # 측정 CSV · 그래프 PNG · raw/(thread_scaling 3회 원본)
├── scripts/
│   └── plot_results.py     # 측정 CSV → matplotlib 그래프
├── data/
│   └── games.csv           # 매치 데이터
├── docs/                   # 계획서, 아키텍처 다이어그램 초안
└── build/                  # 빌드 산출물 (make가 생성)
```

---

## 데이터셋 준비

`data/games.csv`는 **제출물에 포함**되어 있어 별도 다운로드 없이 바로 실행 가능하다.

1. Kaggle [datasnaek/league-of-legends](https://www.kaggle.com/datasets/datasnaek/league-of-legends) 접속
2. `games.csv`를 받아 `data/games.csv`로 저장 (필요 시 `champion_info.json`도 동일 위치에)

| 항목 | 내용 |
|---|---|
| 매치 수 | 51,490 게임 |
| 구조 | 한 행 = 한 매치 (`t1_champ1id`~`t1_champ5id`, `t2_champ1id`~`t2_champ5id`, `winner`) |
| 챔피언 종류 | 138종 |
| 승자 컬럼 | `winner` (1 = 팀1 승, 2 = 팀2 승) |

---

## 빌드 방법

Linux / WSL(Ubuntu) 기준. 의존성 설치:

```bash
sudo apt update
sudo apt install -y build-essential pkg-config \
    libgrpc++-dev libprotobuf-dev protobuf-compiler protobuf-compiler-grpc
```

빌드 (proto 코드 생성 → 컴파일까지 자동):

```bash
make -j4          # server + client + benchmark 전부 빌드 → build/ 아래 생성
make server       # 서버만
make client       # 클라이언트만
make benchmark    # 벤치마크 클라이언트만
make clean        # build/ 삭제
```

그래프 생성용 Python 의존성 (Python 3.9+):

```bash
pip install matplotlib
```

---

## 실행 방법

서버는 `data/games.csv`를 상대 경로로 읽으므로 **저장소 루트에서 실행**한다.

```bash
# 1) 서버 실행 (기본 포트 50051, OpenMP 스레드 수는 기동 시점에 결정)
OMP_NUM_THREADS=8 ./build/analyzer_server

# 2) 조합 분석 질의 (별도 터미널)
./build/analyzer_client --champions 8,432,96,11,112 --r 500
#   --champions  아군 조합 (챔피언 id, 쉼표 구분)
#   --enemies    적군 조합 (생략 시 시너지-only 모드)
#   --r          부트스트랩 리샘플 횟수 (0 = 점추정만, 신뢰구간 생략)

# 3) 픽 추천 질의 (후보 챔피언별 기대 승률 top-10)
./build/analyzer_client --champions 8,432,96,11 --recommend
```

출력 예: `win_rate = 0.51 ci = [0.49, 0.53]`

### Docker Compose 실행 (선택)

`Dockerfile`(멀티스테이지) + `docker-compose.yml`로 서버/클라이언트를 컨테이너로 띄울 수 있다.
`data/`는 읽기 전용으로 마운트되므로, 먼저 `data/games.csv`를 준비한다.

```bash
docker compose up -d server                                          # 서버 백그라운드 기동 (OMP_NUM_THREADS=8)
docker compose run --rm client --champions 8,432,96,11,112 --r 500   # 조합 분석 질의
docker compose run --rm client --champions 8,432,96,11 --recommend   # 픽 추천 질의
docker compose down                                                  # 정리
```

클라이언트는 `SERVER_ADDR=server:50051` 환경변수로 compose 네트워크상의 서버를 찾는다.

---

## 실험 재현 절차

전체 재현은 명령 3개:

```bash
make -j4                              # 1) 빌드
bash eval/benchmark.sh all            # 2) 실험 4종 스윕 (서버 기동/종료 자동)
python3 scripts/plot_results.py      # 3) eval/results/*.csv → 그래프 PNG
```

`benchmark.sh`는 실험별 개별 실행도 지원하고, 요청 수를 환경변수로 조절할 수 있다:

```bash
bash eval/benchmark.sh thread         # 실험 1만
bash eval/benchmark.sh serialize      # 실험 4만 (서버 불필요, 클라이언트 단독)
REQUESTS=10 WARMUP=2 bash eval/benchmark.sh all   # 가볍게 빠른 확인
```

### 실험 구성

| # | 실험 | 변수 | 고정 조건 | 산출물 |
|---|---|---|---|---|
| 1 | Thread scaling | OpenMP 스레드 1/2/4/8 | 클라이언트 1, R=1000 (서버를 스레드 수마다 재기동) | `thread_scaling.csv` → speedup 그래프 |
| 2 | Concurrency scaling | 동시 클라이언트 1/2/4/8/16 | OMP=전체 코어, R=500 | `concurrency.csv` → latency/throughput 그래프 |
| 3 | Compute intensity | 부트스트랩 R=0/100/500/1000 | 클라이언트 1, OMP=전체 코어 | `intensity.csv` → 연산 강도 곡선 |
| 4 | Serialization microbench | protobuf 직렬화/역직렬화 10⁶회 | 서버·채널 없이 클라이언트 단독 | `serialization.csv` (직렬화 µs/op, 그래프 없음) |

실험 4는 protobuf marshaling 비용만 격리 측정해, R=0 통신 바닥값(실험 3) 안에서
직렬화가 차지하는 몫을 분리하기 위한 것이다 (network vs serialization 분해).

### 측정 방법

- 클라이언트 스레드 전원이 atomic start gate에서 **동시 출발** (open 시점 일치)
- 스레드당 워밍업 요청 5개 폐기 (채널/TCP 수립 비용 제거)
- latency는 mean / p50 / p95 / p99 percentile로 집계
- 실험 1(thread scaling)은 세션 간 절대값 변동을 줄이기 위해 **3회 측정 중앙값**을 사용한다
  (개별 실행은 `eval/results/raw/`에 보관 — `thread_scaling_run1~3.csv`)

벤치마크 CSV 컬럼: `clients,r,ok,errors,throughput_rps,mean_ms,p50_ms,p95_ms,p99_ms`
(`thread_scaling.csv`는 맨 앞에 `omp_threads` 추가)

생성 그래프: `thread_scaling.png`, `concurrency_latency.png`,
`concurrency_throughput.png`, `intensity.png` (모두 `eval/results/`)

### 결정적 재현 (BOOTSTRAP_SEED)

부트스트랩 리샘플링은 기본적으로 비결정적이다 (스레드마다 매 실행 새 RNG 시드).
동일한 승률·신뢰구간을 재현하려면 `BOOTSTRAP_SEED`를 고정해 서버를 기동한다.

```bash
# 같은 (시드, 스레드 수) → win_rate / ci 완전 동일
BOOTSTRAP_SEED=42 OMP_NUM_THREADS=4 ./build/analyzer_server
```

결과는 **`(BOOTSTRAP_SEED, OMP_NUM_THREADS)` 조합에 대해 결정적**이다. 스레드 수가
바뀌면 리샘플 분배가 달라지므로, 동일 출력을 얻으려면 **둘 다 고정**해야 한다.
미설정 시 각 스레드가 매번 새 시드를 뽑아 기존대로 비결정적으로 동작한다.

> 성능 측정값(latency·throughput·speedup)은 연산량이 시드와 무관하므로 시드 고정 여부에
> 영향받지 않는다. 시드 고정은 분석 *출력값*의 재현성을 위한 것이다.
