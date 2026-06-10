#include <iostream>
#include <string>
#include <sstream>
#include <vector>

#include <grpcpp/grpcpp.h>
#include "analyzer.grpc.pb.h"   // 생성된 stub: analyzer::AnalyzerService::Stub


static std::vector<int> parseIds(const std::string& csv) {
    std::vector<int> ids;
    std::stringstream ss(csv);
    std::string token;
    while (std::getline(ss, token, ',')){
        ids.push_back(std::stoi(token));
    }
    return ids;
}

int main(int argc, char** argv) {
    std::string target = "localhost:50051";

    // 기본값: 임시 조합. 나중에 --champions / --enemies / --r 플래그로 덮어쓴다.
    std::vector<int> ally  = {1, 2, 3, 4, 5};
    std::vector<int> enemy = {};
    int bootstrap_r = 0;

    for (int i = 1; i < argc; ++i){
        std::string a = argv[i];
        if (a == "--champions" && i + 1 < argc) ally = parseIds(argv[++i]);
        else if (a == "--enemies" && i + 1 < argc) enemy = parseIds(argv[++i]);
        else if (a == "--r" && i + 1 < argc) bootstrap_r = std::stoi(argv[++i]);
        else std::cerr << "Unknown flag: " << a << "\n";
    }

    // 1. 채널 생성
    auto channel = grpc::CreateChannel(target, grpc::InsecureChannelCredentials());

    // 2. Stub 생성
    // 반환 타입은 std::unique_ptr<analyzer::AnalyzerService::Stub>
    auto stub = analyzer::AnalyzerService::NewStub(channel);

    // 3. 요청 메시지 빌드 
    analyzer::CompRequest req;
    for (int id : ally) req.add_ally_ids(id);
    for (int id : enemy) req.add_enemy_ids(id);
    req.set_bootstrap_r(bootstrap_r);

    // 4. RPC 호출
    analyzer::CompResponse res;
    grpc::ClientContext ctx;        // 요청 1건당 컨텍스트 (타임아웃/메타데이터 담는 곳)
    grpc::Status status;            // 호출 결과 상태
    status = stub->AnalyzeComposition(&ctx, req, &res);

    // 5. 결과 처리 / 출력
    if (status.ok()){
        std::cout << "win_rate = " << res.win_rate() 
                  << " ci = [" << res.ci_low() << ", " << res.ci_high() << "]\n";
    } else{
        std::cerr << "RPC failed: " << status.error_code()
                  << " " << status.error_message() << "\n";
        return 1;
    }

    std::cout << "Client connected to " << target << "\n";
    return 0;
}
