#ifndef SERVER_COMMUNICATION_H
#define SERVER_COMMUNICATION_H

#include <grpcpp/grpcpp.h>
#include "server_communication.grpc.pb.h"

using grpc::ServerContext;
using grpc::Status;
using leaftest::ServerCommunication;
using leaftest::TimeRequest;
using leaftest::TimeResponse;
using leaftest::ForwardPassRequest;
using leaftest::ForwardPassResponse;
using leaftest::GradientRequest;
using leaftest::GradientResponse;

class ServerCommunicationServiceImpl final : public ServerCommunication::Service {
public:
    Status GetServerTime(ServerContext* /*context*/, const TimeRequest* /*request*/, TimeResponse* response) override;
    Status ForwardPass(ServerContext* /*context*/, const ForwardPassRequest* request, ForwardPassResponse* response) override;
    Status GetGradients(ServerContext* /*context*/, const GradientRequest* request, GradientResponse* response) override;
};

#endif // SERVER_COMMUNICATION_H 