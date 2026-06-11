# build: proto 생성 + server/client/benchmark 컴파일 
FROM ubuntu:24.04 AS build

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential pkg-config \
        libgrpc++-dev libprotobuf-dev \
        protobuf-compiler protobuf-compiler-grpc \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY Makefile ./
COPY proto/  proto/
COPY server/ server/
COPY client/ client/
RUN make -j"$(nproc)"

# server 
FROM build AS server
WORKDIR /app
EXPOSE 50051
ENTRYPOINT ["./build/analyzer_server"]

# client 
FROM build AS client
WORKDIR /app
ENTRYPOINT ["./build/analyzer_client"]
