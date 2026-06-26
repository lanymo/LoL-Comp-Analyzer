# 시스템 아키텍처

## 1. 전체 블록 다이어그램

클라이언트와 서버가 gRPC(네트워크 경계)로 분리되고, 서버 내부에서 **요청 수준 병렬성(gRPC 워커 스레드)** 과 **연산 수준 병렬성(OpenMP)** 두 층위가 겹친다.

```mermaid
flowchart TB
    subgraph CLIENT["클라이언트 프로세스"]
        direction TB
        C1["benchmark.cc<br/>동시 N 클라이언트 스레드"]
        C2["main.cc<br/>단일 질의 CLI"]
    end

    subgraph NET["── gRPC / HTTP2 · protobuf 직렬화 경계 ──"]
        REQ["CompRequest<br/>ally_ids, enemy_ids, bootstrap_r"]
        RES["CompResponse<br/>win_rate, ci_low, ci_high"]
    end

    subgraph SERVER["서버 프로세스 (0.0.0.0:50051)"]
        direction TB
        POOL["gRPC ServerBuilder<br/>워커 스레드 풀<br/>(요청 수준 병렬성)"]

        subgraph SVC["AnalyzerServiceImpl"]
            A1["AnalyzeComposition()"]
            A2["RecommendPick()"]
        end

        subgraph ENGINE["StatsEngine (읽기 전용 공유, 평생 1개)"]
            direction TB
            IDX["synergy_stats_ / matchup_stats_<br/>(기동 시 1회 구축)"]
            BOOT["bootstrapWinRate()<br/>#pragma omp parallel for<br/>(연산 수준 병렬성)"]
        end
    end

    DATA[("data/games.csv<br/>51,490 매치")]

    C1 -->|stub 호출| REQ
    C2 -->|stub 호출| REQ
    REQ --> POOL
    POOL --> A1 & A2
    A1 --> BOOT
    A2 --> BOOT
    BOOT -->|"synergy/matchup 조회"| IDX
    BOOT --> RES
    RES --> C1 & C2
    DATA -.->|"init(): loadCSV + buildIndex<br/>(기동 시 1회)"| IDX

    classDef boundary fill:#fff,stroke:#888,stroke-dasharray:5 5;
    class NET boundary
```

**두 층위 병렬성**

| | 요청 수준 (gRPC 워커) | 연산 수준 (OpenMP) |
|---|---|---|
| 병렬 단위 | 클라이언트 요청 | 부트스트랩 리샘플 r |
| 담당 주체 | gRPC 워커 스레드 풀 | OpenMP 스레드 |
| 공유 데이터 | `StatsEngine` 인덱스 (읽기 전용 → 경합 적음) | `games_` 리샘플 (읽기 전용) |
| 동기화 | 없음 (무상태 요청) | thread-local map → 병합 / per-thread RNG |
| 측정 축 | 동시 클라이언트 수 → latency·throughput | OpenMP 스레드 수 → speedup |


---

## 2. 요청 처리 시퀀스 (1건의 AnalyzeComposition)

부트스트랩 단계에서 통신 → CPU로 병목이 이동하는 지점을 보여준다.

```mermaid
sequenceDiagram
    autonumber
    participant CL as 클라이언트
    participant GR as gRPC 워커 스레드
    participant SV as AnalyzerServiceImpl
    participant EN as StatsEngine
    participant OMP as OpenMP 스레드들

    CL->>GR: CompRequest (protobuf 역직렬화)
    GR->>SV: AnalyzeComposition(ctx, req, res)
    SV->>EN: bootstrapWinRate(ally, enemy, R)

    Note over EN,OMP: #pragma omp parallel — 리샘플 R회 분배
    par 스레드별 독립 실행
        EN->>OMP: r = 0 .. R/T (thread-local map + RNG)
        OMP-->>EN: scores[r]
    end

    Note over EN: 단일 스레드 병합<br/>mean + 95% percentile CI 정렬
    EN-->>SV: win_rate, ci_low, ci_high
    SV-->>GR: CompResponse 채움
    GR-->>CL: protobuf 직렬화 → 응답
```

---

## 3. 기동 시 초기화 흐름 (1회성)

```mermaid
flowchart LR
    START(["server/main.cc 시작"]) --> INIT["service.init('data/games.csv')"]
    INIT --> LOAD["loadCSV()<br/>51,490행 파싱 → games_"]
    LOAD --> BUILD["buildIndex()<br/>#pragma omp parallel for<br/>매치당 35쌍 집계"]
    BUILD --> MERGE["thread-local map 병합<br/>→ synergy_stats_ / matchup_stats_"]
    MERGE --> LISTEN["BuildAndStart() → :50051 listen"]
    LOAD -->|"로드 실패"| ABORT["return 1 (서버 중단)"]
```

---