#include "stats_engine.h"
#include <omp.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <string>

using namespace std;

// ─── Key helpers ────────────────────────────────────────────────────────────

uint64_t StatsEngine::synergyKey(int a, int b) {
    if (a > b) swap(a, b);
    return ((uint64_t)(uint32_t)a << 32) | (uint32_t)b;
}

uint64_t StatsEngine::matchupKey(int a, int b) {
    return ((uint64_t)(uint32_t)a << 32) | (uint32_t)b;
}

// ─── CSV Loader ──────────────────────────────────────────────────────────────

bool StatsEngine::loadCSV(const string& path) {
    ifstream file(path);
    if (!file.is_open()) {
        cerr << "[StatsEngine] Cannot open: " << path << "\n";
        return false;
    }

    // Parse header row
    string line;
    if (!getline(file, line)) return false;
    if (!line.empty() && line.back() == '\r') line.pop_back();

    vector<string> header;
    {
        istringstream iss(line);
        string tok;
        while (getline(iss, tok, ',')) header.push_back(tok);
    }

    auto colIdx = [&](const string& name) -> int {
        for (int i = 0; i < (int)header.size(); ++i)
            if (header[i] == name) return i;
        return -1;
    };

    int col_winner = colIdx("winner");
    array<int, 5> col_t1, col_t2;
    for (int i = 0; i < 5; ++i) {
        col_t1[i] = colIdx("t1_champ" + to_string(i + 1) + "id");
        col_t2[i] = colIdx("t2_champ" + to_string(i + 1) + "id");
    }

    if (col_winner < 0) {
        cerr << "[StatsEngine] 'winner' column not found\n";
        return false;
    }
    for (int i = 0; i < 5; ++i) {
        if (col_t1[i] < 0 || col_t2[i] < 0) {
            cerr << "[StatsEngine] champ column not found at index " << i << "\n";
            return false;
        }
    }

    // Parse data rows
    games_.clear();
    games_.reserve(55000);

    int skipped = 0;
    while (getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        // Tokenise row into integer fields
        vector<int> fields;
        fields.reserve(header.size());
        {
            istringstream iss(line);
            string tok;
            while (getline(iss, tok, ',')) {
                try { fields.push_back(stoi(tok)); }
                catch (...) { fields.push_back(0); }
            }
        }

        int n = (int)fields.size();
        if (col_winner >= n) { ++skipped; continue; }

        Game g;
        g.winner = fields[col_winner];
        if (g.winner != 1 && g.winner != 2) { ++skipped; continue; }

        bool ok = true;
        for (int i = 0; i < 5 && ok; ++i) {
            if (col_t1[i] >= n || col_t2[i] >= n) { ok = false; break; }
            g.t1[i] = fields[col_t1[i]];
            g.t2[i] = fields[col_t2[i]];
            if (g.t1[i] <= 0 || g.t2[i] <= 0) ok = false; // id=0 means missing
        }
        if (!ok) { ++skipped; continue; }

        games_.push_back(g);
    }

    cout << "[StatsEngine] Loaded " << games_.size()
              << " games (skipped " << skipped << ")\n";
    return !games_.empty();
}

// ─── Index builder ───────────────────────────────────────────────────────────

void StatsEngine::buildIndex() {
    int n       = (int)games_.size();
    int nthreads = omp_get_max_threads();

    // Allocate one map per thread to avoid false-sharing / locking
    vector<unordered_map<uint64_t, PairStat>> local_syn(nthreads);
    vector<unordered_map<uint64_t, PairStat>> local_mat(nthreads);

    #pragma omp parallel for schedule(static)
    for (int i = 0; i < n; ++i) {
        int tid = omp_get_thread_num();
        const Game& g = games_[i];
        bool t1_won = (g.winner == 1);

        // ── Synergy pairs, team 1 ──
        for (int a = 0; a < 5; ++a) {
            for (int b = a + 1; b < 5; ++b) {
                auto& s = local_syn[tid][synergyKey(g.t1[a], g.t1[b])];
                ++s.total;
                if (t1_won) ++s.wins;
            }
        }

        // ── Synergy pairs, team 2 ──
        for (int a = 0; a < 5; ++a) {
            for (int b = a + 1; b < 5; ++b) {
                auto& s = local_syn[tid][synergyKey(g.t2[a], g.t2[b])];
                ++s.total;
                if (!t1_won) ++s.wins;
            }
        }

        // ── Matchup pairs (directional: t1_champ vs t2_champ) ──
        for (int a = 0; a < 5; ++a) {
            for (int b = 0; b < 5; ++b) {
                auto& s = local_mat[tid][matchupKey(g.t1[a], g.t2[b])];
                ++s.total;
                if (t1_won) ++s.wins;
            }
        }
    }

    // Merge thread-local maps into global maps
    synergy_stats_.clear();
    matchup_stats_.clear();

    for (int t = 0; t < nthreads; ++t) {
        for (auto& kv : local_syn[t]) {
            synergy_stats_[kv.first].wins  += kv.second.wins;
            synergy_stats_[kv.first].total += kv.second.total;
        }
        for (auto& kv : local_mat[t]) {
            matchup_stats_[kv.first].wins  += kv.second.wins;
            matchup_stats_[kv.first].total += kv.second.total;
        }
    }

    cout << "[StatsEngine] Index built: "
              << synergy_stats_.size() << " synergy pairs, "
              << matchup_stats_.size() << " matchup pairs\n";
}

// ─── Point-estimate score (no resampling) ────────────────────────────────────

double StatsEngine::scoreComposition(const vector<int>& ally_ids,
                                     const vector<int>& enemy_ids) const {
    double sum = 0.0;
    int    cnt = 0;

    int n_ally = (int)ally_ids.size();

    for (int a = 0; a < n_ally; ++a) {
        for (int b = a + 1; b < n_ally; ++b) {
            auto it = synergy_stats_.find(synergyKey(ally_ids[a], ally_ids[b]));
            if (it != synergy_stats_.end() && it->second.total > 0) {
                sum += it->second.winRate();
                ++cnt;
            }
        }
    }

    for (int a : ally_ids) {
        for (int e : enemy_ids) {
            auto it = matchup_stats_.find(matchupKey(a, e));
            if (it != matchup_stats_.end() && it->second.total > 0) {
                sum += it->second.winRate();
                ++cnt;
            }
        }
    }

    return cnt > 0 ? sum / cnt : 0.5;
}

// ─── Bootstrap confidence interval ───────────────────────────────────────────

double StatsEngine::bootstrapWinRate(const vector<int>& ally_ids,
                                     const vector<int>& enemy_ids,
                                     int R,
                                     double* ci_low,
                                     double* ci_high) const {
    int n = (int)games_.size();
    if (n == 0 || R == 0) {
        double s = scoreComposition(ally_ids, enemy_ids);
        *ci_low = *ci_high = s;
        return s;
    }

    // Pre-compute query pair keys so each thread can allocate exactly-sized maps
    vector<uint64_t> syn_keys, mat_keys;
    {
        int na = (int)ally_ids.size();
        for (int a = 0; a < na; ++a)
            for (int b = a + 1; b < na; ++b)
                syn_keys.push_back(synergyKey(ally_ids[a], ally_ids[b]));
        for (int a : ally_ids)
            for (int e : enemy_ids)
                mat_keys.push_back(matchupKey(a, e));
    }

    vector<double> scores(R);

    #pragma omp parallel
    {
        // Per-thread RNG (seeded uniquely per thread)
        mt19937 rng(random_device{}() ^
                         (uint32_t)(omp_get_thread_num() * 2654435761u));
        uniform_int_distribution<int> dist(0, n - 1);

        // Pre-allocate thread-local pair stat maps with exact capacity
        unordered_map<uint64_t, PairStat> syn_r, mat_r;
        syn_r.reserve(syn_keys.size() * 2);
        mat_r.reserve(mat_keys.size() * 2);

        #pragma omp for schedule(static)
        for (int r = 0; r < R; ++r) {
            // Reset maps to query keys with zero stats
            syn_r.clear();
            mat_r.clear();
            for (uint64_t k : syn_keys) syn_r[k] = {};
            for (uint64_t k : mat_keys) mat_r[k] = {};

            // Resample N games with replacement
            for (int i = 0; i < n; ++i) {
                const Game& g = games_[dist(rng)];
                bool t1_won = (g.winner == 1);

                // Check synergy pairs for team 1
                for (int a = 0; a < 5; ++a) {
                    for (int b = a + 1; b < 5; ++b) {
                        auto it = syn_r.find(synergyKey(g.t1[a], g.t1[b]));
                        if (it != syn_r.end()) {
                            ++it->second.total;
                            if (t1_won) ++it->second.wins;
                        }
                    }
                }

                // Check synergy pairs for team 2
                for (int a = 0; a < 5; ++a) {
                    for (int b = a + 1; b < 5; ++b) {
                        auto it = syn_r.find(synergyKey(g.t2[a], g.t2[b]));
                        if (it != syn_r.end()) {
                            ++it->second.total;
                            if (!t1_won) ++it->second.wins;
                        }
                    }
                }

                // Check matchup pairs (t1 vs t2, directional)
                for (int a = 0; a < 5; ++a) {
                    for (int b = 0; b < 5; ++b) {
                        auto it = mat_r.find(matchupKey(g.t1[a], g.t2[b]));
                        if (it != mat_r.end()) {
                            ++it->second.total;
                            if (t1_won) ++it->second.wins;
                        }
                    }
                }
            }

            // Aggregate pair win rates for this resample
            double sum = 0.0;
            int    cnt = 0;
            for (auto& kv : syn_r) if (kv.second.total > 0) { sum += kv.second.winRate(); ++cnt; }
            for (auto& kv : mat_r) if (kv.second.total > 0) { sum += kv.second.winRate(); ++cnt; }
            scores[r] = (cnt > 0) ? sum / cnt : 0.5;
        }
    }

    // Mean and 95% percentile CI
    double mean = 0.0;
    for (double s : scores) mean += s;
    mean /= R;

    sort(scores.begin(), scores.end());
    *ci_low  = scores[max(0, (int)(R * 0.025))];
    *ci_high = scores[min(R - 1, (int)(R * 0.975))];

    return mean;
}
