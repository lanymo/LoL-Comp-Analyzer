// tests/engine_test.cc
// StatsEngine 단위 테스트 — gRPC/네트워크 없이 계산 엔진 검증
// 실행:  make test  

#include "stats_engine.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

static int g_failures = 0;

#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            std::printf("  [FAIL] %s:%d  %s\n", __FILE__, __LINE__, #cond); \
            ++g_failures;                                                 \
        }                                                                 \
    } while (0)

static bool approx(double a, double b, double eps = 1e-9) {
    return std::fabs(a - b) <= eps;
}

// 손으로 승률을 계산할 수 있는 최소 합성 데이터셋을 임시 CSV로 사용
static std::string writeFixture() {
    const std::string path = "engine_test_fixture.csv";
    std::ofstream f(path);
    f << "winner,"
         "t1_champ1id,t1_champ2id,t1_champ3id,t1_champ4id,t1_champ5id,"
         "t2_champ1id,t2_champ2id,t2_champ3id,t2_champ4id,t2_champ5id\n";
    f << "1,1,2,3,4,5,6,7,8,9,10\n";   // 게임 A: 팀1 승
    f << "2,1,2,3,4,5,6,7,8,9,10\n";   // 게임 B: 팀2 승
    f << "1,1,2,3,4,5,6,7,8,9,10\n";   // 게임 C: 팀1 승
    return path;
}

int main() {
    const std::string path = writeFixture();

    StatsEngine engine;
    CHECK(engine.loadCSV(path));
    CHECK(engine.gameCount() == 3);
    engine.buildIndex();

    const std::vector<int> ally  = {1, 2, 3, 4, 5};
    const std::vector<int> enemy = {6, 7, 8, 9, 10};

    // 1) scoreComposition (시너지-only): 모든 아군 쌍 2/3 → 평균 2/3
    CHECK(approx(engine.scoreComposition(ally, {}), 2.0 / 3.0));

    // 2) scoreComposition (매치업 포함): 시너지·매치업 모두 2/3 → 2/3
    CHECK(approx(engine.scoreComposition(ally, enemy), 2.0 / 3.0));

    // 3) 등장한 적 없는 쌍 → 0.5 폴백
    //    (1,6)은 같은 팀인 적이 없다(1=항상 팀1, 6=항상 팀2) → miss → 0.5
    CHECK(approx(engine.scoreComposition({1, 6}, {}), 0.5));

    // 4) bootstrap R=0 → 점추정과 정확히 일치, CI 폭 0
    {
        double lo = -1.0, hi = -1.0;
        const double mean = engine.bootstrapWinRate(ally, enemy, 0, &lo, &hi);
        CHECK(approx(mean, 2.0 / 3.0));
        CHECK(approx(lo, mean) && approx(hi, mean));
    }

    // 5) bootstrap R>0 → 구조적 불변식만 검증(정확값은 RNG 의존)
    //    0 ≤ ci_low ≤ ci_high ≤ 1, mean ∈ [0,1]
    {
        double lo = -1.0, hi = -1.0;
        const double mean = engine.bootstrapWinRate(ally, enemy, 200, &lo, &hi);
        CHECK(lo >= 0.0 && hi <= 1.0 && lo <= hi);
        CHECK(mean >= 0.0 && mean <= 1.0);
    }

    // 6) recommendPick: 이미 뽑힌 챔피언 제외 + 정확히 top_k개 + 점수 범위
    {
        const auto picks = engine.recommendPicks({1, 2}, {6, 7}, 3);
        CHECK(picks.size() == 3u);
        for (const auto& pr : picks) {
            CHECK(pr.first != 1 && pr.first != 2 && pr.first != 6 && pr.first != 7);
            CHECK(pr.second >= 0.0 && pr.second <= 1.0);
        }
    }

    std::remove(path.c_str());

    if (g_failures == 0) {
        std::printf("[engine_test] ALL PASSED\n");
        return 0;
    }
    std::printf("[engine_test] %d CHECK(S) FAILED\n", g_failures);
    return 1;
}
