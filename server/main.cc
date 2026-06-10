#include <iostream>
#include <string>
#include <grpcpp/grpcpp.h>
#include "analyzer_service.h"

int main(int argc, char** argv) {
    // TODO: parse port from argv
    std::string address = "0.0.0.0:50051";

    AnalyzerServiceImpl service;
    grpc::ServerBuilder builder;

    if (!service.init("data/games.csv")){
        std:: cerr << "fail to initialize service\n";
        return 1;
    }

    int selected_port = 0;  // 바인딩 성공 시 실제 포트, 실패 시 0
    builder.AddListeningPort(address, grpc::InsecureServerCredentials(), &selected_port);
    builder.RegisterService(&service);

    auto server = builder.BuildAndStart();
    if (!server || selected_port == 0) {
        std::cerr << "Failed to bind/start server on " << address
                  << " (port already in use?)\n";
        return 1;
    }
    std::cout << "Server listening on " << address << std::endl;
    server->Wait();
    return 0;
}
