#include "stats_engine.h"
#include <omp.h>

// TODO: load games.csv into memory at startup

// Aggregate pairwise win/loss counts across all games (OpenMP parallelized)
void StatsEngine::buildIndex(const std::vector<Game>& games) {
    // TODO: parallel reduction over games
    #pragma omp parallel for schedule(dynamic)
    for (int i = 0; i < (int)games.size(); ++i) {
        // stub
    }
}

// Bootstrap confidence interval for a given champion set
double StatsEngine::bootstrapWinRate(
    const std::vector<int>& champ_ids, int R)
{
    // TODO: R bootstrap resamples (OpenMP)
    (void)champ_ids;
    (void)R;
    return 0.0;
}
