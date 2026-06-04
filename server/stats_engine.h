#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

using namespace std;

struct Game {
    int t1[5];
    int t2[5];
    int winner; // 1 = team1, 2 = team2
};

struct PairStat {
    int wins  = 0;
    int total = 0;
    double winRate() const { return total > 0 ? (double)wins / total : 0.5; }
};

class StatsEngine {
public:
    // Must call loadCSV → buildIndex before any queries.
    bool loadCSV(const string& path);
    void buildIndex();

    // Point estimate from pre-built index (no resampling).
    double scoreComposition(const vector<int>& ally_ids,
                            const vector<int>& enemy_ids) const;

    // 95% bootstrap CI.  enemy_ids may be empty (synergy-only mode).
    // Returns mean win rate; writes CI endpoints to *ci_low / *ci_high.
    double bootstrapWinRate(const vector<int>& ally_ids,
                            const vector<int>& enemy_ids,
                            int R,
                            double* ci_low,
                            double* ci_high) const;

    int gameCount() const { return static_cast<int>(games_.size()); }

    const unordered_map<uint64_t, PairStat>& synergyStats() const { return synergy_stats_; }
    const unordered_map<uint64_t, PairStat>& matchupStats()  const { return matchup_stats_; }

private:
    vector<Game> games_;

    // synergy_stats_: sorted pair key (min<<32|max) → wins = that pair's team won
    unordered_map<uint64_t, PairStat> synergy_stats_;

    // matchup_stats_: directional key (t1_champ<<32|t2_champ) → wins = t1_champ's team won
    unordered_map<uint64_t, PairStat> matchup_stats_;

    static uint64_t synergyKey(int a, int b); // order-independent
    static uint64_t matchupKey(int a, int b); // directional: a (ally) vs b (enemy)
};
