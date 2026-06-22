#include "analyzer_service.h"

#include <algorithm>   // std::clamp

#define all(x) (x).begin(), (x).end()

// 요청 1건이 유발할 수 있는 연산량 통제
static constexpr int kMaxBootstrapR = 2000;

bool AnalyzerServiceImpl::init(const std::string& csv_path){
    if (!engine_.loadCSV(csv_path)) return false;
    engine_.buildIndex();
    return true;
}

grpc::Status AnalyzerServiceImpl::AnalyzeComposition(
    grpc::ServerContext* ctx,
    const analyzer::CompRequest* req,
    analyzer::CompResponse* res)
{
    (void)ctx;
    std::vector<int> ally(all(req->ally_ids()));
    std::vector<int> enemy(all(req->enemy_ids()));

    const int R = std::clamp(req->bootstrap_r(), 0, kMaxBootstrapR);

    double lo = 0.0, hi = 0.0;
    double mean = engine_.bootstrapWinRate(ally, enemy, R, &lo, &hi);
    res->set_win_rate(mean);
    res->set_ci_low(lo);
    res->set_ci_high(hi);
    return grpc::Status::OK;
}

grpc::Status AnalyzerServiceImpl::RecommendPick(
    grpc::ServerContext* ctx,
    const analyzer::PickRequest* req,
    analyzer::PickResponse* res)
{
    (void)ctx;
    std::vector<int> ally(all(req->ally_ids()));
    std::vector<int> enemy(all(req->enemy_ids()));

    constexpr int kTopK = 10;
    for (const auto& [id, score] : engine_.recommendPicks(ally, enemy, kTopK)) {
        res->add_recommended_ids(id);
        res->add_scores(score);
    }
    return grpc::Status::OK;
}
