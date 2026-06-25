#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <cmath>

#include <grpcpp/grpcpp.h>
#include "analyzer.grpc.pb.h"  

using Clock = std::chrono::steady_clock; // 시간 간격 측정용 clock
using Ms    = std::chrono::duration<double, std::milli>;

struct Config {
    std::string target   = "localhost:50051";
    int clients          = 8;     // 동시 클라이언트
    int requests         = 100;   // 스레드당 요청 수
    int warmup           = 5;     // 스레드당 버릴 워밍업 요청 수(연결 수립 비용 제거)
    int bootstrap_r      = 500;   // 서버 부트스트랩 R           → 연산 강도 축
    std::vector<int> ally  = {1, 2, 3, 4, 5};
    std::vector<int> enemy = {};
};

// 한 thread 당 결과: latency 측정용
struct ThreadResult {
    std::vector<double> latencies_ms;  // 워밍업 제외, 성공한 요청만
    int errors = 0;
};

static std::vector<int> parseIds(const std::string& csv) {
    std::vector<int> ids;
    std::stringstream ss(csv);
    std::string tok;
    while (std::getline(ss, tok, ',')) ids.push_back(std::stoi(tok));
    return ids;
}

static Config parseArgs(int argc, char** argv) {
    Config c;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() { return std::string(argv[++i]); };
        if      (a == "--target"    && i + 1 < argc) c.target      = next();
        else if (a == "--clients"   && i + 1 < argc) c.clients     = std::stoi(next());
        else if (a == "--requests"  && i + 1 < argc) c.requests    = std::stoi(next());
        else if (a == "--warmup"    && i + 1 < argc) c.warmup      = std::stoi(next());
        else if (a == "--r"         && i + 1 < argc) c.bootstrap_r = std::stoi(next());
        else if (a == "--champions" && i + 1 < argc) c.ally        = parseIds(next());
        else if (a == "--enemies"   && i + 1 < argc) c.enemy       = parseIds(next());
        else std::cerr << "Unknown flag: " << a << "\n";
    }
    return c;
}

static void worker(const Config& cfg, std::atomic<bool>* start_gate, ThreadResult* out) {
    auto channel = grpc::CreateChannel(cfg.target, grpc::InsecureChannelCredentials());
    auto stub    = analyzer::AnalyzerService::NewStub(channel);

    analyzer::CompRequest req;
    for (int id : cfg.ally)  req.add_ally_ids(id);
    for (int id : cfg.enemy) req.add_enemy_ids(id);
    req.set_bootstrap_r(cfg.bootstrap_r);

    out->latencies_ms.reserve(cfg.requests);

    // 모든 스레드가 준비될 때까지 스핀 대기 → 동시 출발
    while (!start_gate->load(std::memory_order_acquire)) {}

    int total = cfg.warmup + cfg.requests;
    for (int n = 0; n < total; ++n) {
        analyzer::CompResponse res;
        grpc::ClientContext ctx;

        auto t0 = Clock::now();
        grpc::Status st = stub->AnalyzeComposition(&ctx, req, &res);
        auto t1 = Clock::now();

        if (!st.ok()) {
            ++out->errors;
            // 처음 몇 건만 stderr로 진단 출력
            if (out->errors <= 3)
                std::cerr << "[bench] RPC error: code=" << st.error_code()
                          << " msg=\"" << st.error_message() << "\"\n";
            continue;
        }
        if (n < cfg.warmup) continue; 
        out->latencies_ms.push_back(Ms(t1 - t0).count());
    }
}

// percentile 헬퍼. 요청 한 건 시간
static double percentile(const std::vector<double>& sorted, double p) {
    if (sorted.empty()) return 0.0;
    size_t idx = static_cast<size_t>(p * (sorted.size() - 1));
    return sorted[idx];
}

static void runBenchmark(const Config& cfg) {
    std::vector<ThreadResult> results(cfg.clients);
    std::vector<std::thread>  threads;
    std::atomic<bool> start_gate{false};

    for (int i = 0; i < cfg.clients; ++i)
        threads.emplace_back(worker, std::cref(cfg), &start_gate, &results[i]);

    auto wall0 = Clock::now();
    start_gate.store(true, std::memory_order_release);
    for (auto& t : threads) t.join();
    auto wall1 = Clock::now();

    std::vector<double> all;
    int errors = 0;
    for (auto& r : results) {
        all.insert(all.end(), r.latencies_ms.begin(), r.latencies_ms.end());
        errors += r.errors;
    }
    std::sort(all.begin(), all.end());

    double wall_s     = std::chrono::duration<double>(wall1 - wall0).count();
    double throughput = all.size() / wall_s;                      
    double mean = 0.0; for (double x : all) mean += x;
    if (!all.empty()) mean /= all.size();

    std::cout << cfg.clients << ","
              << cfg.bootstrap_r << ","
              << all.size() << ","
              << errors << ","
              << throughput << ","
              << mean << ","
              << percentile(all, 0.50) << ","
              << percentile(all, 0.95) << ","
              << percentile(all, 0.99) << "\n";
}

int main(int argc, char** argv) {
    Config cfg = parseArgs(argc, argv);
    runBenchmark(cfg);
    return 0;
}
