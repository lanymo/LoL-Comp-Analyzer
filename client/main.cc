#include <iostream>
#include <string>
#include <vector>

#include <grpcpp/grpcpp.h>
#include "analyzer.grpc.pb.h"   // 생성된 stub: analyzer::AnalyzerService::Stub


static std::vector<int> parseIds(const std::string& csv) {
    std::vector<int> ids;
    
    // TODO: csv를 ','로 잘라서 각 토큰을 std::stoi로 int 변환 후 push_back
    //  힌트: std::stringstream + std::getline(ss, token, ',') 패턴이 가장 쉬움.
    //        REST로 치면 쿼리스트링 "?ids=1,2,3"를 파싱하는 것과 같은 일.
    return ids;
}

int main(int argc, char** argv) {
    std::string target = "localhost:50051";

    // 기본값: 임시 조합. 나중에 --champions / --enemies / --r 플래그로 덮어쓴다.
    std::vector<int> ally  = {1, 2, 3, 4, 5};
    std::vector<int> enemy = {};
    int bootstrap_r = 0;

    // TODO(1): argv를 순회하며 플래그 파싱
    //   --champions 1,2,3,4,5  → ally  = parseIds(...)
    //   --enemies   6,7,8,9,10 → enemy = parseIds(...)
    //   --r 500                → bootstrap_r = std::stoi(...)
    //   힌트: for (int i = 1; i < argc; ++i) { std::string a = argv[i]; ... }
    //         지금은 건너뛰고 기본값으로 먼저 통신부터 성공시켜도 됨.

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
