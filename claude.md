# CLAUDE.md — LoL Champion Composition Analyzer

> 이 파일은 프로젝트의 전체 맥락을 Claude(또는 AI 어시스턴트)에게 전달하기 위한 문서다.
> 새 대화를 시작할 때 이 파일을 첨부하면 기존 논의를 처음부터 다시 설명할 필요가 없다.

---

## 프로젝트 개요

- **과목:** Parallel & Distributed Computing (CSEG414)
- **유형:** 학부생 단독 프로젝트
- **마감:** 2026-06-28 (일)
- **스택:** gRPC + OpenMP (C++)
- **주제:** LoL 랭크 매치 데이터 기반 챔피언 조합 분석 서비스

### 핵심 설계 철학

- **"smaller but well-evaluated > ambitious but incomplete"** — 과제 문서의 원칙.
- 2단 파이프라인: **1단계(B, 필수)**가 단독 완성품, **2단계(C, 보너스)**는 떼어내도 무너지지 않는 독립 모듈.
- **미들웨어 = 시스템의 본질:** "요청-응답 분석 서비스"이므로 gRPC 통신이 곧 기능. 미들웨어가 명목이 아닌 필수 요소.

---

## 기술 아키텍처

### 시스템 구조

```
Client (gRPC) ──protobuf──▶ Server (C++ gRPC, 다중 워커 스레드)
                                │
                    ┌───────────┴───────────┐
                    ▼                       ▼
              1단계 (필수)            2단계 (보너스)
              쌍 기반 승률 분석       유사도 + 군집화
              OpenMP 병렬화          OpenMP 병렬화
                    │                       │
                    └───────────┬───────────┘
                                ▼
                         protobuf 응답 ──▶ Client
```

### gRPC 서비스 (.proto)

- `AnalyzeComposition(req) → resp` — 챔피언 조합의 승률 + 신뢰구간
- `RecommendPick(req) → resp` — 후보 챔피언별 기대 승률 병렬 평가
- (보너스) `FindSimilar(req) → resp` — 유사도 기반 군집 응답

### 두 층위의 병렬성 (보고서 핵심 분석 포인트)

| | 요청 수준 (gRPC 동시성) | 연산 수준 (OpenMP) |
|---|---|---|
| 단위 | 클라이언트 요청 | 매치 / 쌍 / 리샘플 |
| 담당 | gRPC 워커 스레드 | OpenMP 스레드 |
| 측정 | 동시 요청 수 vs latency/throughput | 스레드 수 vs speedup |
| 경합 | 읽기 전용 데이터 공유(경합 적음) | thread-local 후 병합 |

두 층위가 같은 코어를 경쟁하므로, 워커 스레드 ↑ vs OpenMP 스레드 ↑ 트레이드오프 분석이 보고서 차별점.

---

## 1단계 (B) — 필수: 쌍 기반 승률 분석

- 서버 기동 시 매치 CSV를 메모리에 로드
- 질의 → 관련 매치에서 시너지 C(5,2)=10쌍 + 매치업 5×5=25쌍 = 35쌍/매치 집계
- 부트스트랩 R=500~1000회 리샘플링 → 승률 신뢰구간 추정 (CPU-bound의 1차 원천)
- 병렬화: `#pragma omp parallel for`로 리샘플 분배, thread-local 누적 후 병합

## 2단계 (C) — 보너스: 유사도 + 군집화

- 1단계 산출물(승률 벡터)만 입력으로 사용 → 1단계와 완전 분리 가능
- 챔피언 쌍 (i,j) 코사인/유클리드 거리 → M×M 행렬, O(M²)
- 상삼각 쌍을 1차원 인덱스로 평탄화 → `#pragma omp parallel for schedule(static)`
- 시간 부족 시 제거해도 1단계가 독립 완성품으로 남음

---

## 데이터셋

### 확정: datasnaek/league-of-legends (Kaggle)

- **URL:** https://www.kaggle.com/datasets/datasnaek/league-of-legends
- **규모:** ~51,490 매치, 61개 컬럼
- **구조:** 한 행 = 한 매치. `t1_champ1id` ~ `t1_champ5id`, `t2_champ1id` ~ `t2_champ5id`, `winner` 가 한 레코드에 완결
- **메모리:** ~수십 MB. 전부 메모리에 상주 가능. L3 캐시 근처에 머물러 순수 CPU-bound 측정에 유리

### 선택 근거

- 한 행 = 한 매치 구조라 C++ 파싱이 가장 간단하고 전처리 불필요
- 연산 무게는 데이터 크기가 아니라 부트스트랩 R로 조절 (R=1000이면 질의당 ~1.75억 쌍 연산)
- 수십 MB 메모리는 "작은 게" 아니라 이 설계에서 "깨끗한 것" — 디스크 I/O 노이즈 없이 순수 speedup 측정 가능

### 대안 데이터셋 (필요 시 교체 카드)

- **ezalos/lol-victory-prediction-from-champion-selection** — 챔피언 픽 + 승패에 집중, 인게임 통계 최소
- **jakubkrasuski/league-of-legends-match-dataset-2025** — 최신 메타 반영, 챔피언 ~170종

---

## 성능 평가 계획

### 필수 실험

1. **Thread scaling:** 질의 1건을 1/2/4/8 OpenMP 스레드로 처리할 때 speedup
2. **Concurrency scaling:** 동시 클라이언트 1/2/4/8/16일 때 평균 latency·throughput
3. **연산 강도:** 부트스트랩 R=0/100/500/1000으로 늘리며 병렬 이득 증가 곡선

### 필수 그래프

- Speedup vs OpenMP 스레드 수
- Latency vs 동시 클라이언트 수
- Throughput vs 동시 클라이언트 수

### 병목 분석 포인트

- protobuf 직렬화/역직렬화 오버헤드 vs 순수 연산 시간 비율
- 부트스트랩 OFF→ON 시 병목이 통신에서 CPU로 이동하는 지점 (**보고서 하이라이트**)
- gRPC 워커와 OpenMP가 같은 코어를 경쟁할 때의 상호 간섭

---

## 채점 기준 (100점)

| 항목 | 배점 |
|---|---|
| System Architecture & Middleware Design | 30 |
| Parallel Computing & Acceleration | 30 |
| Performance Evaluation & Analysis | 25 |
| Reproducibility & Documentation | 15 |

---

## 마일스톤

| 시점 | 목표 |
|---|---|
| ~5월 말 | 데이터셋 확보, gRPC 환경 구축, **gRPC "hello world" 성공** |
| ~6월 초 | 매치 데이터 로드 + 단일 스레드 쌍 집계(대조군) |
| ~6월 중순 | 1단계 OpenMP 가속 완성 → **여기까지로 제출 가능** |
| ~6월 20일 | (여유 시) 2단계 유사도/군집화 + 추천 질의 |
| ~6월 21일 | 성능 측정 + matplotlib 그래프 |
| ~6월 28일 | 최종 PDF 보고서 제출 |

---

## Git 전략

### Repo 이름

```
pdc-grpc-lol-analytics
```

### 브랜치 구조

```
main        ← 항상 빌드되고 서버 기동 가능한 상태만 유지
dev         ← 일상 작업 브랜치 (여기서 push하면서 작업)
feature/stage2-similarity   ← 2단계 시작할 때만 추가
```

- PR 없이 `dev`에서 직접 push하면서 작업
- 마일스톤마다 `dev` → `main` merge + 태그

### 커밋 메시지 컨벤션

태그를 앞에 붙여서 `git log --oneline`으로 한눈에 파악 가능하게:

```
[proto]   .proto 정의, protobuf 관련
[server]  gRPC 서버 로직
[client]  gRPC 클라이언트 로직
[stage1]  1단계 — 쌍 집계, 부트스트랩, OpenMP
[stage2]  2단계 — 유사도, 군집화
[eval]    성능 측정, 벤치마크
[data]    데이터 로드, 파싱
[build]   Makefile, Docker, 빌드 설정
[docs]    README, 보고서, 문서
```

예시:
```
[proto] define AnalyzeComposition and RecommendPick service
[server] load match CSV into memory on startup
[stage1] implement pair counting with thread-local maps
[stage1] add OpenMP parallelization for bootstrap loop
[eval] add latency measurement per request
[docs] update README with experiment reproduction steps
```

### 태그 전략

마일스톤마다 태그를 남겨서 복구 지점 확보:

```bash
git tag v0.1-grpc-hello-world      # gRPC 통신 성공
git tag v0.2-data-loader            # 데이터 로드 완성
git tag v1.0-stage1-complete        # 1단계 완성 (제출 가능 상태)
git tag v1.1-stage2-similarity      # 2단계 추가
git tag v2.0-final                  # 최종 제출
```

### 워크플로우

```bash
# 평소 작업
git checkout dev
git add -p
git commit -m "[stage1] add bootstrap resampling R=500"
git push origin dev

# 마일스톤 달성 시
git checkout main
git merge dev
git tag v1.0-stage1-complete
git push origin main --tags
```

---

## 과제 요구사항 체크리스트

### 제출물

- [ ] 소스 코드 전체
- [ ] 빌드 스크립트 (Makefile, docker-compose.yml)
- [ ] README.md (빌드 방법, 실행 방법, 실험 재현 방법)
- [ ] 최종 보고서 PDF

### 보고서 필수 섹션

- [ ] Abstract & Problem Definition
- [ ] System Architecture Diagram (블록 다이어그램)
- [ ] Implementation Details (미들웨어 설정, 직렬화, 스레드 구조, 동기화, 병렬화 전략)
- [ ] Performance Evaluation & Bottleneck Analysis (그래프 포함)

### 기술 요구사항

- [ ] Parallel Computing: OpenMP
- [ ] Distributed Middleware: gRPC
- [ ] 배포: localhost / Docker Compose (단일 머신 허용)

---

## 리스크 & 완화

| 리스크 | 완화 전략 |
|---|---|
| gRPC 빌드 연동 어려움 | 가장 먼저 "hello world" 통신 성공시킴 (5월 말) |
| 스코프 초과 | 1단계를 6월 중순까지 독립 완성, 2단계는 분리 모듈 |
| 연산이 너무 가벼움 | 부트스트랩 R≥500으로 CPU-bound 확보 |
| 두 층위 병렬성 간섭 | 분석 소재로 활용 (트레이드오프 측정) |

---

## 이전 설계 결정 기록

- **Kafka → gRPC 전환:** "스트림 처리"보다 "요청-응답 분석 서비스"에 자연스러움. 통신이 곧 기능이라 미들웨어 명목성 해소.
- **연산 로직은 미들웨어 변경과 무관:** 쌍 분해·부트스트랩·유사도 로직은 그대로 유지.
- **메모리 크기(수십 MB)는 문제 아님:** 성능 평가 대상은 데이터 크기가 아니라 질의당 CPU 연산량. R로 연산 무게 조절.

---

## 디렉토리 구조 (예상)

```
pdc-grpc-lol-analytics/
├── proto/
│   └── lol_analytics.proto
├── src/
│   ├── server/
│   │   ├── main.cc
│   │   ├── data_loader.cc / .h
│   │   ├── pair_analyzer.cc / .h      # 1단계
│   │   ├── bootstrap.cc / .h          # 1단계
│   │   └── similarity.cc / .h         # 2단계 (보너스)
│   └── client/
│       └── main.cc
├── data/
│   └── (CSV 파일 — .gitignore에 추가, README에 다운로드 안내)
├── eval/
│   ├── benchmark.sh
│   └── plot_results.py
├── Makefile
├── docker-compose.yml
├── Dockerfile
├── README.md
├── CLAUDE.md                           # 이 파일
└── .gitignore
```
