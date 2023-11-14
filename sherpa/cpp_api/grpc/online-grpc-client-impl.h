// Copyright (c) 2021 Ximalaya Speech Team (Xiang Lyu)
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GRPC_GRPC_CLIENT_H_
#define GRPC_GRPC_CLIENT_H_

#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "grpc/grpc.h"
#include "grpcpp/channel.h"
#include "grpcpp/client_context.h"
#include "grpcpp/create_channel.h"

#include "sherpa/csrc/log.h"
#include "sherpa.grpc.pb.h"

namespace sherpa {

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReaderWriter;
using sherpa::ASR;
using sherpa::Request;
using sherpa::Response;

class GrpcClient {
 public:
  GrpcClient(const std::string& host,
             int32_t port,
             int32_t nbest,
             const std::string& reqid);

  void SendBinaryData(const void* data, size_t size);
  void ReadLoopFunc();
  void Join();
  bool done() const { return done_; }
  std::string key_;

 private:
  void Connect();
  std::string host_;
  int port_;
  int nbest_;
  std::string reqid_;
  std::shared_ptr<Channel> channel_;
  std::unique_ptr<ASR::Stub> stub_;
  std::shared_ptr<ClientContext> context_;
  std::unique_ptr<ClientReaderWriter<Request, Response>> stream_;
  std::shared_ptr<Request> request_;
  std::shared_ptr<Response> response_;
  bool done_ = false;
  std::unique_ptr<std::thread> t_;
};

}  // namespace sherpa

#endif  // GRPC_GRPC_CLIENT_H_

