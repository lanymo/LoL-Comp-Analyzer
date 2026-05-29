# LoL 챔피언 조합 분석 서비스

**과제:** CSEG414/5414 Parallel & Distributed Computing — Course Project  
**마감:** 2026-06-28  
**스택:** C++ + gRPC + OpenMP  

---

## 디렉터리 구조

```
lol-comp-analyzer/
├── README.md               # 이 파일 (빌드/실행/실험 절차)
├── proto/
│   └── analyzer.proto      # gRPC 서비스 정의 (protobuf)
├── server/
│   ├── main.cc             # gRPC 서버 진입점
│   ├── analyzer_service.cc # AnalyzeComposition / RecommendPick 구현
│   └── stats_engine.cc     # 쌍 집계 + 부트스트랩 (OpenMP 병렬화)
├── client/
│   ├── main.cc             # gRPC 클라이언트 (질의 발행 + 결과 출력)
│   └── benchmark.cc        # 동시 다중 클라이언트 벤치마크
├── data/
│   └── games.csv           # 원본 매치 데이터 (51,490 게임)
├── scripts/
│   ├── preprocess.py       # (필요시) CSV → 바이너리 전처리
│   └── plot_results.py     # matplotlib 그래프 생성
├── report/
│   └── proposal_grpc_openmp.md  # 프로젝트 계획서
├── Makefile                # 빌드 설정
└── docker-compose.yml      # 서버/클라이언트 컨테이너 분리 (선택)
```

---

## 데이터셋 개요

| 항목 | 내용 |
|---|---|
| 파일 | `data/games.csv` |
| 매치 수 | 51,490 게임 |
| 구조 | 한 행 = 한 매치 (팀1 챔피언 5개 + 팀2 챔피언 5개 + 승패) |
| 챔피언 종류 | 138종 |
| 승자 컬럼 | `winner` (1 = 팀1 승, 2 = 팀2 승) |

---

## 빌드 방법 (TODO)

```bash
# gRPC + protobuf 설치 후
make -j4          # server + client + benchmark 전부 빌드
make server       # 서버만
make client       # 클라이언트만
make clean        # build/ 삭제
```

## 실행 방법 (TODO)

```bash
# 서버 실행
./server/analyzer_server

# 클라이언트 실행 (별도 터미널)
./client/analyzer_client --champions 8,432,96,11,112
```

---

## 실험 계획

1. **Thread scaling:** OpenMP 스레드 1/2/4/8 × speedup
2. **Concurrency scaling:** 동시 클라이언트 1/2/4/8/16 × latency, throughput
3. **연산 강도:** 부트스트랩 R=0/100/500/1000 × latency
