#include <iostream>
#include <vector>
#include <grpcpp/grpcpp.h>
// #include "analyzer.grpc.pb.h"

int main(int argc, char** argv) {
    // TODO: parse --champions flag from argv
    std::string target = "localhost:50051";

    auto channel = grpc::CreateChannel(target, grpc::InsecureChannelCredentials());
    // auto stub = analyzer::AnalyzerService::NewStub(channel);

    // TODO: build CompRequest and call AnalyzeComposition
    std::cout << "Client connected to " << target << std::endl;
    return 0;
}
