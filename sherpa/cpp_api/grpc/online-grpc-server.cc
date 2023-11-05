// sherpa/cpp_api/grpc/online-grpc-server.cc
//
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>

#include "asio.hpp"
#include "sherpa/cpp_api/grpc/online-grpc-server-impl.h"
#include "sherpa/csrc/log.h"
#include "torch/all.h"

static constexpr const char *kUsageMessage = R"(
Automatic speech recognition with sherpa using grpc.

Usage:

sherpa-online-grpc-server --help

sherpa-online-grpc-server \
  --use-gpu=false \
  --port=6006 \
  --num-work-threads=5 \
  --nn-model=/path/to/cpu.jit \
  --tokens=/path/to/tokens.txt \
  --decoding-method=greedy_search \
  --log-file=./log.txt
)";

int32_t main(int32_t argc, char *argv[]) {
  torch::set_num_threads(1);
  torch::set_num_interop_threads(1);
  torch::NoGradGuard no_grad;

  torch::jit::getExecutorMode() = false;
  torch::jit::getProfilingMode() = false;
  torch::jit::setGraphExecutorOptimize(false);

  sherpa::ParseOptions po(kUsageMessage);

  sherpa::OnlineWebsocketServerConfig config;

  // the server will listen on this port, for both websocket and http
  int32_t port = 6006;

  // size of the thread pool for handling network connections
  int32_t num_io_threads = 1;

  // size of the thread pool for neural network computation and decoding
  int32_t num_work_threads = 5;

  int32_t num_workers = 4;

  po.Register("num-io-threads", &num_io_threads,
              "Number of threads to use for network connections.");

  po.Register("num-work-threads", &num_work_threads,
              "Number of threads to use for neural network "
              "computation and decoding.");

  po.Register("port", &port, "The port on which the server will listen.");

  config.Register(&po);

  if (argc == 1) {
    po.PrintUsage();
    exit(EXIT_FAILURE);
  }

  po.Read(argc, argv);

  if (po.NumArgs() != 0) {
    SHERPA_LOG(ERROR) << "Unrecognized positional arguments!";
    po.PrintUsage();
    exit(EXIT_FAILURE);
  }

  config.Validate();

  asio::io_context io_conn;  // for network connections
  asio::io_context io_work;  // for neural network and decoding

  sherpa::OnlineGrpcServer server(io_conn, io_work, config);
  server.Run();


  // SHERPA_LOG(INFO) << "Number of I/O threads: " << num_io_threads << "\n";
  SHERPA_LOG(INFO) << "Number of work threads: " << num_work_threads << "\n";

  // give some work to do for the io_work pool
  auto work_guard = asio::make_work_guard(io_work);

  std::vector<std::thread> io_threads;

  // decrement since the main thread is also used for network communications
  for (int32_t i = 0; i < num_io_threads - 1; ++i) {
    io_threads.emplace_back([&io_conn]() { io_conn.run(); });
  }

  std::vector<std::thread> work_threads;
  for (int32_t i = 0; i < num_work_threads; ++i) {
    work_threads.emplace_back([&io_work]() { io_work.run(); });
  }

  io_conn.run();

  grpc::EnableDefaultHealthCheckService(true);
  grpc::reflection::InitProtoReflectionServerBuilderPlugin();
  ServerBuilder builder;
  std::string address("0.0.0.0:" + std::to_string(port));
  builder.AddListeningPort(address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  builder.SetSyncServerOption(ServerBuilder::SyncServerOption::NUM_CQS,
                              num_workers);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  SHERPA_LOG(INFO) << "Listening on: " << port << "\n";

  for (auto &t : io_threads) {
    t.join();
  }

  for (auto &t : work_threads) {
    t.join();
  }

  server->Wait();
  return 0;
}
