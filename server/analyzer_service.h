#pragma once

#include <string>

#include "analyzer.grpc.pb.h"
#include "stats_engine.h"

class AnalyzerServiceImpl : public analyzer::AnalyzerService::Service {
public:
    // 서버 기동 시 1번 호출: CSV 로드 + 인덱스 구축. 성공 시 true.
    bool init(const std::string& csv_path);

    // gRPC가 요청마다 호출하는 메서드 (base의 virtual을 override)
    grpc::Status AnalyzeComposition(
        grpc::ServerContext* ctx,
        const analyzer::CompRequest* req,        // 들어온 요청 (읽기 전용)
        analyzer::CompResponse* res) override;    // 채워서 돌려줄 응답

    grpc::Status RecommendPick(
        grpc::ServerContext* ctx,
        const analyzer::PickRequest* req,
        analyzer::PickResponse* res) override;

private:
    StatsEngine engine_;   // 계산 엔진 (서버가 소유, 평생 1개)
};
