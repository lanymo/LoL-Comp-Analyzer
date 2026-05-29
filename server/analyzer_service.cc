#include "analyzer_service.h"

// TODO: implement AnalyzeComposition
grpc::Status AnalyzerServiceImpl::AnalyzeComposition(
    grpc::ServerContext* ctx,
    const analyzer::CompRequest* req,
    analyzer::CompResponse* res)
{
    (void)ctx;
    // stub: pass champion IDs to stats_engine
    res->set_win_rate(0.0);
    res->set_confidence(0.0);
    return grpc::Status::OK;
}

// TODO: implement RecommendPick
grpc::Status AnalyzerServiceImpl::RecommendPick(
    grpc::ServerContext* ctx,
    const analyzer::PickRequest* req,
    analyzer::PickResponse* res)
{
    (void)ctx;
    // stub: return empty recommendation
    return grpc::Status::OK;
}
