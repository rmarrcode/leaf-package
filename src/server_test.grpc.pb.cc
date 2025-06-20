// Generated by the gRPC C++ plugin.
// If you make any local change, they will be lost.
// source: server_test.proto

#include "server_test.pb.h"
#include "server_test.grpc.pb.h"

#include <functional>
#include <grpcpp/support/async_stream.h>
#include <grpcpp/support/async_unary_call.h>
#include <grpcpp/impl/channel_interface.h>
#include <grpcpp/impl/client_unary_call.h>
#include <grpcpp/support/client_callback.h>
#include <grpcpp/support/message_allocator.h>
#include <grpcpp/support/method_handler.h>
#include <grpcpp/impl/rpc_service_method.h>
#include <grpcpp/support/server_callback.h>
#include <grpcpp/impl/server_callback_handlers.h>
#include <grpcpp/server_context.h>
#include <grpcpp/impl/service_type.h>
#include <grpcpp/support/sync_stream.h>
#include <grpcpp/ports_def.inc>
namespace leaftest {

static const char* ServerTest_method_names[] = {
  "/leaftest.ServerTest/GetServerTime",
};

std::unique_ptr< ServerTest::Stub> ServerTest::NewStub(const std::shared_ptr< ::grpc::ChannelInterface>& channel, const ::grpc::StubOptions& options) {
  (void)options;
  std::unique_ptr< ServerTest::Stub> stub(new ServerTest::Stub(channel, options));
  return stub;
}

ServerTest::Stub::Stub(const std::shared_ptr< ::grpc::ChannelInterface>& channel, const ::grpc::StubOptions& options)
  : channel_(channel), rpcmethod_GetServerTime_(ServerTest_method_names[0], options.suffix_for_stats(),::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  {}

::grpc::Status ServerTest::Stub::GetServerTime(::grpc::ClientContext* context, const ::leaftest::TimeRequest& request, ::leaftest::TimeResponse* response) {
  return ::grpc::internal::BlockingUnaryCall< ::leaftest::TimeRequest, ::leaftest::TimeResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), rpcmethod_GetServerTime_, context, request, response);
}

void ServerTest::Stub::async::GetServerTime(::grpc::ClientContext* context, const ::leaftest::TimeRequest* request, ::leaftest::TimeResponse* response, std::function<void(::grpc::Status)> f) {
  ::grpc::internal::CallbackUnaryCall< ::leaftest::TimeRequest, ::leaftest::TimeResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_GetServerTime_, context, request, response, std::move(f));
}

void ServerTest::Stub::async::GetServerTime(::grpc::ClientContext* context, const ::leaftest::TimeRequest* request, ::leaftest::TimeResponse* response, ::grpc::ClientUnaryReactor* reactor) {
  ::grpc::internal::ClientCallbackUnaryFactory::Create< ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_GetServerTime_, context, request, response, reactor);
}

::grpc::ClientAsyncResponseReader< ::leaftest::TimeResponse>* ServerTest::Stub::PrepareAsyncGetServerTimeRaw(::grpc::ClientContext* context, const ::leaftest::TimeRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderHelper::Create< ::leaftest::TimeResponse, ::leaftest::TimeRequest, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), cq, rpcmethod_GetServerTime_, context, request);
}

::grpc::ClientAsyncResponseReader< ::leaftest::TimeResponse>* ServerTest::Stub::AsyncGetServerTimeRaw(::grpc::ClientContext* context, const ::leaftest::TimeRequest& request, ::grpc::CompletionQueue* cq) {
  auto* result =
    this->PrepareAsyncGetServerTimeRaw(context, request, cq);
  result->StartCall();
  return result;
}

ServerTest::Service::Service() {
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      ServerTest_method_names[0],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< ServerTest::Service, ::leaftest::TimeRequest, ::leaftest::TimeResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(
          [](ServerTest::Service* service,
             ::grpc::ServerContext* ctx,
             const ::leaftest::TimeRequest* req,
             ::leaftest::TimeResponse* resp) {
               return service->GetServerTime(ctx, req, resp);
             }, this)));
}

ServerTest::Service::~Service() {
}

::grpc::Status ServerTest::Service::GetServerTime(::grpc::ServerContext* context, const ::leaftest::TimeRequest* request, ::leaftest::TimeResponse* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}


}  // namespace leaftest
#include <grpcpp/ports_undef.inc>

