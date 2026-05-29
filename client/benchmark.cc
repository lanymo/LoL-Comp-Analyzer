#include <iostream>
#include <thread>
#include <vector>
#include <chrono>

// Spawn N concurrent gRPC clients and measure latency / throughput
void runBenchmark(int num_clients, int requests_per_client) {
    // TODO: create threads, each owning a stub, fire requests, collect timings
    (void)num_clients;
    (void)requests_per_client;
}

int main(int argc, char** argv) {
    // TODO: parse --clients and --requests flags
    int clients  = 8;
    int requests = 100;
    runBenchmark(clients, requests);
    return 0;
}
