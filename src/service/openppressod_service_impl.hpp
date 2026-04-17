#ifndef OPENPPRESSOD_SERVICE_IMPL_HPP
#define OPENPPRESSOD_SERVICE_IMPL_HPP

#include <memory>

#include <grpcpp/support/server_callback.h>
#include <libopenpresso/interfaces/libopenpresso_core.hpp>
#include <openpresso_proto/openpresso.grpc.pb.h>
#include <openpresso_proto/openpresso.pb.h>

namespace openpressod
{

class AsyncEventDispatcher;

class OpenpressodServiceImpl : public OpenpressoDaemon::CallbackService {
public:
  OpenpressodServiceImpl(const std::shared_ptr<AsyncEventDispatcher>& dispatcher,
                         libopenpresso::CorePtr core);

  grpc::ServerUnaryReactor* resetScales(grpc::CallbackServerContext* context,
                                        const google::protobuf::Empty* request,
                                        google::protobuf::Empty* response) override;

  grpc::ServerUnaryReactor* getBrewProfileInfo(grpc::CallbackServerContext* context,
                                               const google::protobuf::Empty* request,
                                               BrewProfileInfo* response) override;

  grpc::ServerUnaryReactor* setBrewProfile(grpc::CallbackServerContext* context,
                                           const BrewProfile* request,
                                           SetProfileResult* response) override;

  grpc::ServerUnaryReactor* setPowerState(grpc::CallbackServerContext* context,
                                          const PowerState* request,
                                          google::protobuf::Empty* response) override;

  grpc::ServerUnaryReactor* getPowerState(grpc::CallbackServerContext* context,
                                          const google::protobuf::Empty* request,
                                          PowerState* response) override;

  grpc::ServerUnaryReactor* startBrew(grpc::CallbackServerContext* context,
                                      const google::protobuf::Empty* request,
                                      google::protobuf::Empty* response) override;

  grpc::ServerUnaryReactor* stopBrew(grpc::CallbackServerContext* context,
                                     const google::protobuf::Empty* request,
                                     google::protobuf::Empty* response) override;

  grpc::ServerUnaryReactor* getBrewState(grpc::CallbackServerContext* context,
                                         const google::protobuf::Empty* request,
                                         BrewState* response) override;

  grpc::ServerUnaryReactor* setSteamModeState(grpc::CallbackServerContext* context,
                                              const SteamModeState* request,
                                              google::protobuf::Empty* response) override;

  grpc::ServerUnaryReactor* getSteamModeState(grpc::CallbackServerContext* context,
                                              const google::protobuf::Empty* request,
                                              SteamModeState* response) override;

  grpc::ServerWriteReactor<Event>* events(grpc::CallbackServerContext* context,
                                          const google::protobuf::Empty* request) override;

  grpc::ServerWriteReactor<Metrics>* metrics(grpc::CallbackServerContext* context,
                                             const MetricsRequest* request) override;

private:
  static auto makeVoidCallback(grpc::ServerUnaryReactor* reactor);

private:
  std::shared_ptr<AsyncEventDispatcher> m_dispatcher;
  libopenpresso::CorePtr m_core;
};

} // namespace openpressod

#endif // OPENPPRESSOD_SERVICE_IMPL_HPP