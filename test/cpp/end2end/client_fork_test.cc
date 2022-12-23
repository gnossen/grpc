// Copyright 2022 The gRPC Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <grpc/support/port_platform.h>

#ifndef GRPC_ENABLE_FORK_SUPPORT
// No-op for builds without fork support.
int main(int /* argc */, char** /* argv */) { return 0; }
#else  // GRPC_ENABLE_FORK_SUPPORT

#include <pthread.h>
#include <signal.h>
#include <stdlib.h>

#include <gtest/gtest.h>

#include "absl/strings/str_cat.h"
#include "absl/time/clock.h"

#include <grpc/fork.h>
#include <grpc/grpc.h>
#include <grpc/support/log.h>
#include <grpc/support/time.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include "src/core/lib/gprpp/fork.h"
#include "src/proto/grpc/testing/echo.grpc.pb.h"
#include "test/core/util/port.h"
#include "test/core/util/test_config.h"

namespace grpc {
namespace testing {
namespace {

using ::grpc::testing::EchoRequest;
using ::grpc::testing::EchoResponse;
using ::grpc::testing::EchoTestService;

class ServiceImpl final : public EchoTestService::Service {
  Status BidiStream(
      ServerContext* /*context*/,
      ServerReaderWriter<EchoResponse, EchoRequest>* stream) override {
    EchoRequest request;
    EchoResponse response;
    while (stream->Read(&request)) {
      gpr_log(GPR_INFO, "recv msg %s", request.message().c_str());
      response.set_message(request.message());
      stream->Write(response);
      gpr_log(GPR_INFO, "wrote msg %s", response.message().c_str());
    }
    return Status::OK;
  }
};

std::unique_ptr<EchoTestService::Stub> MakeStub(const std::string& addr) {
  return EchoTestService::NewStub(
      grpc::CreateChannel(addr, InsecureChannelCredentials()));
}


void run_gdb(pid_t pid) {
  // TODO: Use something better than the systemc ommand.
  char gdb_cmd[1024];
  sprintf(gdb_cmd, "sudo gdb -ex 'set confirm off' -ex 'echo attaching' -ex 'attach %ld' -ex 'echo print_backtrace' -ex 'thread apply all bt' -ex 'echo printed_backtrace' -ex 'quit'", (long)pid);
  gpr_log(GPR_INFO, "Running `%s`", gdb_cmd);
  system(gdb_cmd);
  gpr_log(GPR_INFO, "gdb exited");
}

// Returns false on timeout.
bool wait_pid_timeout(pid_t pid, const absl::Duration timeout) {
  absl::SleepFor(timeout);
  int status;
  int result = waitpid(pid, &status, WNOHANG);
  return result >= pid;
}

TEST(ClientForkTest, ClientCallsBeforeAndAfterForkSucceed) {
  grpc_core::Fork::Enable(true);

  int port = grpc_pick_unused_port_or_die();
  std::string addr = absl::StrCat("localhost:", port);

  pid_t server_pid = fork();
  switch (server_pid) {
    case -1:  // fork failed
      GTEST_FAIL() << "failure forking";
    case 0:  // post-fork child
    {
      ServiceImpl impl;
      grpc::ServerBuilder builder;
      builder.AddListeningPort(addr, grpc::InsecureServerCredentials());
      builder.RegisterService(&impl);
      std::unique_ptr<Server> server(builder.BuildAndStart());
      server->Wait();
      return;
    }
    default:  // post-fork parent
      break;
  }

  // Do a round trip before we fork.
  {
    std::unique_ptr<EchoTestService::Stub> stub = MakeStub(addr);
    EchoRequest request;
    EchoResponse response;
    ClientContext context;
    context.set_wait_for_ready(true);

    auto stream = stub->BidiStream(&context);

    request.set_message("Hello");
    ASSERT_TRUE(stream->Write(request));
    ASSERT_TRUE(stream->Read(&response));
    ASSERT_EQ(response.message(), request.message());
  }
  // Fork and do round trips in the post-fork parent and child.
  pid_t child_client_pid = fork();
  switch (child_client_pid) {
    case -1:  // fork failed
      FAIL() << "fork failed";
    case 0:  // post-fork child
    {
      gpr_log(GPR_DEBUG, "DO NOT SUBMIT:In post-fork child");
      EchoRequest request;
      EchoResponse response;
      ClientContext context;
      context.set_wait_for_ready(true);

      gpr_log(GPR_DEBUG, "DO NOT SUBMIT: child Creating stub");
      std::unique_ptr<EchoTestService::Stub> stub = MakeStub(addr);
      gpr_log(GPR_DEBUG, "DO NOT SUBMIT: child Created stub");
      auto stream = stub->BidiStream(&context);
      gpr_log(GPR_DEBUG, "DO NOT SUBMIT: child Created stub");

      request.set_message("Hello again from child");
      gpr_log(GPR_DEBUG, "DO NOT SUBMIT: doing write from child");
      ASSERT_TRUE(stream->Write(request));
      gpr_log(GPR_DEBUG, "DO NOT SUBMIT: doing read from child");
      ASSERT_TRUE(stream->Read(&response));
      gpr_log(GPR_DEBUG, "DO NOT SUBMIT: comparing messages from child");
      ASSERT_EQ(response.message(), request.message());
      gpr_log(GPR_DEBUG, "DO NOT SUBMIT: child exiting");
      exit(0);
    }
    default:  // post-fork parent
    {
      gpr_log(GPR_DEBUG, "In post-fork parent");
      EchoRequest request;
      EchoResponse response;
      ClientContext context;
      context.set_wait_for_ready(true);

      std::unique_ptr<EchoTestService::Stub> stub = MakeStub(addr);
      auto stream = stub->BidiStream(&context);

      request.set_message("Hello again from parent");
      gpr_log(GPR_DEBUG, "DO NOT SUBMIT: doing write from parent");
      EXPECT_TRUE(stream->Write(request));
      gpr_log(GPR_DEBUG, "DO NOT SUBMIT: doing read from parent");
      EXPECT_TRUE(stream->Read(&response));
      gpr_log(GPR_DEBUG, "DO NOT SUBMIT: comparing messages from parent");
      EXPECT_EQ(response.message(), request.message());

      gpr_log(GPR_DEBUG, "DO NOT SUBMIT: waiting on child to shut down");
      // Wait for the post-fork child to exit; ensure it exited cleanly.

      if (!wait_pid_timeout(child_client_pid, absl::Seconds(10))) {
        run_gdb(child_client_pid);
        ASSERT_FALSE(true) << "Child process timed out.";
      }
    }
  }



  kill(server_pid, SIGINT);
}

}  // namespace
}  // namespace testing
}  // namespace grpc

#endif  // GRPC_ENABLE_FORK_SUPPORT

int main(int argc, char** argv) {
  grpc::testing::TestEnvironment env(&argc, argv);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
