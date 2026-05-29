CXX      := g++
CXXFLAGS := -std=c++17 -O2 -Wall
OMPFLAGS := -fopenmp

# pkg-config로 gRPC / protobuf 플래그 자동 조회
GRPC_FLAGS  := $(shell pkg-config --cflags grpc++ protobuf)
GRPC_LIBS   := $(shell pkg-config --libs   grpc++ grpc protobuf) -lgrpc++_reflection

# protoc / grpc_cpp_plugin 경로 (시스템에 따라 수정)
PROTOC      := protoc
GRPC_PLUGIN := $(shell which grpc_cpp_plugin)

PROTO_DIR  := proto
BUILD_DIR  := build
SERVER_DIR := server
CLIENT_DIR := client

PROTO_SRC  := $(PROTO_DIR)/analyzer.proto
PROTO_GEN  := $(BUILD_DIR)/analyzer.pb.cc $(BUILD_DIR)/analyzer.grpc.pb.cc

.PHONY: all server client benchmark clean proto

all: server client benchmark

# ── protobuf 코드 생성 ─────────────────────────────────────────────
proto: $(PROTO_GEN)

$(BUILD_DIR)/analyzer.pb.cc $(BUILD_DIR)/analyzer.grpc.pb.cc: $(PROTO_SRC) | $(BUILD_DIR)
	$(PROTOC) \
		-I $(PROTO_DIR) \
		--cpp_out=$(BUILD_DIR) \
		--grpc_out=$(BUILD_DIR) \
		--plugin=protoc-gen-grpc=$(GRPC_PLUGIN) \
		$(PROTO_SRC)

# ── server ────────────────────────────────────────────────────────
SERVER_SRCS := $(SERVER_DIR)/main.cc \
               $(SERVER_DIR)/analyzer_service.cc \
               $(SERVER_DIR)/stats_engine.cc \
               $(BUILD_DIR)/analyzer.pb.cc \
               $(BUILD_DIR)/analyzer.grpc.pb.cc

server: proto $(BUILD_DIR)/analyzer_server

$(BUILD_DIR)/analyzer_server: $(SERVER_SRCS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(OMPFLAGS) $(GRPC_FLAGS) \
		-I$(BUILD_DIR) -I$(SERVER_DIR) \
		$(SERVER_SRCS) -o $@ $(GRPC_LIBS)

# ── client ────────────────────────────────────────────────────────
CLIENT_SRCS := $(CLIENT_DIR)/main.cc \
               $(BUILD_DIR)/analyzer.pb.cc \
               $(BUILD_DIR)/analyzer.grpc.pb.cc

client: proto $(BUILD_DIR)/analyzer_client

$(BUILD_DIR)/analyzer_client: $(CLIENT_SRCS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(GRPC_FLAGS) \
		-I$(BUILD_DIR) \
		$(CLIENT_SRCS) -o $@ $(GRPC_LIBS)

# ── benchmark ─────────────────────────────────────────────────────
BENCH_SRCS := $(CLIENT_DIR)/benchmark.cc \
              $(BUILD_DIR)/analyzer.pb.cc \
              $(BUILD_DIR)/analyzer.grpc.pb.cc

benchmark: proto $(BUILD_DIR)/analyzer_benchmark

$(BUILD_DIR)/analyzer_benchmark: $(BENCH_SRCS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(GRPC_FLAGS) \
		-I$(BUILD_DIR) \
		$(BENCH_SRCS) -o $@ $(GRPC_LIBS)

# ── 공통 ──────────────────────────────────────────────────────────
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)
