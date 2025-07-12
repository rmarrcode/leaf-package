#ifndef SERVER_COMMUNICATION_H
#define SERVER_COMMUNICATION_H

#include <grpcpp/grpcpp.h>
#include "server_communication.grpc.pb.h"
#include "model.h"
#include <map>
#include <string>
#include <vector>
#include <memory>

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
    // Store actual Model objects on the server instead of just vectors of floats
    std::map<std::string, std::shared_ptr<Model>> stored_models;

    // NEW: Map to keep the most recent output tensor for each stored model
    std::map<std::string, std::vector<float>> model_outputs;

public:
    Status GetServerTime(ServerContext* /*context*/, const TimeRequest* /*request*/, TimeResponse* response) override;
    Status ForwardPass(ServerContext* /*context*/, const ForwardPassRequest* request, ForwardPassResponse* response) override;
    Status GetGradients(ServerContext* /*context*/, const GradientRequest* request, GradientResponse* response) override;
    Status StoreModelWeights(ServerContext* /*context*/, const StoreModelWeightsRequest* request, StoreModelWeightsResponse* response) override;
    
    // Helper methods for model management
    bool has_model(const std::string& model_id) const;
    std::shared_ptr<Model> get_model(const std::string& model_id) const;
    void store_model(const std::string& model_id, std::shared_ptr<Model> model);
    void remove_model(const std::string& model_id);
    std::vector<std::string> get_stored_model_ids() const;

    // NEW: Store the latest outputs produced by each model after a forward pass
    std::vector<float> get_outputs(const std::string& model_id) const;
};

#endif // SERVER_COMMUNICATION_H 