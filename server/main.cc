#include <iostream>
#include <string>
#include <grpcpp/grpcpp.h>
#include "analyzer_service.h"

int main(int argc, char** argv) {
    // TODO: parse port from argv
    std::string address = "0.0.0.0:50051";

    AnalyzerServiceImpl service;
    grpc::ServerBuilder builder;
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    auto server = builder.BuildAndStart();
    std::cout << "Server listening on " << address << std::endl;
    server->Wait();
    return 0;
}
