#ifndef SERVER_COMMUNICATION_H
#define SERVER_COMMUNICATION_H

#include <grpcpp/grpcpp.h>
#include "server_communication.grpc.pb.h"
#include <map>
#include <string>
#include <vector>

using grpc::ServerContext;
using grpc::Status;
using leaftest::ServerCommunication;
using leaftest::TimeRequest;
using leaftest::TimeResponse;
using leaftest::ForwardPassRequest;
using leaftest::ForwardPassResponse;
using leaftest::GradientRequest;
using leaftest::GradientResponse;
using leaftest::StoreModelWeightsRequest;
using leaftest::StoreModelWeightsResponse;

class ServerCommunicationServiceImpl final : public ServerCommunication::Service {
private:
    // Store model weights on the server
    std::map<std::string, std::vector<float>> stored_models;
    std::map<std::string, std::string> model_types;

public:
    Status GetServerTime(ServerContext* /*context*/, const TimeRequest* /*request*/, TimeResponse* response) override;
    Status ForwardPass(ServerContext* /*context*/, const ForwardPassRequest* request, ForwardPassResponse* response) override;
    Status GetGradients(ServerContext* /*context*/, const GradientRequest* request, GradientResponse* response) override;
    Status StoreModelWeights(ServerContext* /*context*/, const StoreModelWeightsRequest* request, StoreModelWeightsResponse* response) override;
};

#endif // SERVER_COMMUNICATION_H 