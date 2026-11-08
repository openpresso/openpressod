#ifndef OPENPPRESSOD_SERVICE_IMPL_HPP
#define OPENPPRESSOD_SERVICE_IMPL_HPP

#include <memory>
#include <system_error>

#include <google/protobuf/empty.pb.h>
#include <grpcpp/server_context.h>
#include <grpcpp/support/server_callback.h>
#include <grpcpp/support/status.h>
#include <libopenpresso/interfaces/libopenpresso_core.hpp>
#include <openpresso_proto/brew_log.pb.h>
#include <openpresso_proto/brew_profile.pb.h>
#include <openpresso_proto/events.pb.h>
#include <openpresso_proto/machine_state.pb.h>
#include <openpresso_proto/metrics.pb.h>
#include <openpresso_proto/openpresso.grpc.pb.h>
#include <openpresso_proto/user_settings.pb.h>

namespace openpressod
{

class AsyncEventDispatcher;
class BrewLogManager;
class BrewTimer;

namespace db::brew_profiles
{
class AsyncBrewProfilesRepository;
}
namespace db::brew_history
{
class AsyncBrewHistoryRepository;
}

class OpenpressodServiceImpl : public OpenpressoDaemon::CallbackService {
public:
  OpenpressodServiceImpl(std::shared_ptr<AsyncEventDispatcher> dispatcher,
                         std::shared_ptr<BrewLogManager> brewLogManager,
                         libopenpresso::CorePtr core,
                         std::shared_ptr<const BrewTimer> brewTimer,
                         std::shared_ptr<db::brew_profiles::AsyncBrewProfilesRepository> profilesRepository,
                         std::shared_ptr<db::brew_history::AsyncBrewHistoryRepository> historyRepository);

  grpc::ServerUnaryReactor* resetScales(grpc::CallbackServerContext* context,
                                        const google::protobuf::Empty* request,
                                        google::protobuf::Empty* response) override;

  grpc::ServerUnaryReactor* getUserSettings(grpc::CallbackServerContext* context,
                                            const google::protobuf::Empty* request,
                                            UserSettings* response) override;

  grpc::ServerUnaryReactor* setUserSettings(grpc::CallbackServerContext* context,
                                            const UserSettings* request,
                                            google::protobuf::Empty* response) override;

  grpc::ServerUnaryReactor* getActiveBrewProfile(grpc::CallbackServerContext* context,
                                                 const google::protobuf::Empty* request,
                                                 ActiveBrewProfile* response) override;

  grpc::ServerUnaryReactor* setActiveBrewProfile(grpc::CallbackServerContext* context,
                                                 const ActivateBrewProfile* request,
                                                 google::protobuf::Empty* response) override;

  grpc::ServerUnaryReactor* getPersistentBrewProfile(grpc::CallbackServerContext* context,
                                                     const BrewProfileId* request,
                                                     PersistentBrewProfile* response) override;

  grpc::ServerUnaryReactor* addPersistentBrewProfile(grpc::CallbackServerContext* context,
                                                     const BrewProfile* request,
                                                     BrewProfileId* response) override;

  grpc::ServerUnaryReactor* updatePersistentBrewProfile(grpc::CallbackServerContext* context,
                                                        const UpdatePersistentProfile* request,
                                                        google::protobuf::Empty* response) override;

  grpc::ServerUnaryReactor* deletePersistentBrewProfile(grpc::CallbackServerContext* context,
                                                        const BrewProfileId* request,
                                                        google::protobuf::Empty* response) override;

  grpc::ServerUnaryReactor* findPersistentBrewProfiles(grpc::CallbackServerContext* context,
                                                       const FindBrewProfiles* request,
                                                       BrewProfilesSearchResult* response) override;

  grpc::ServerUnaryReactor* findBrewLogs(grpc::CallbackServerContext* context,
                                         const FindBrewLogs* request,
                                         BrewLogsSearchResult* response) override;

  grpc::ServerUnaryReactor* removeBrewLog(grpc::CallbackServerContext* context,
                                          const BrewLogId* request,
                                          google::protobuf::Empty* response) override;

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

  grpc::ServerWriteReactor<BrewLogStream>* getBrewLog(grpc::CallbackServerContext* context,
                                                      const BrewLogId* request) override;

private:
  static auto makeVoidCallback(grpc::ServerUnaryReactor* reactor);
  static grpc::Status dbError(const std::system_error& error);

private:
  std::shared_ptr<AsyncEventDispatcher> m_dispatcher;
  std::shared_ptr<BrewLogManager> m_brewLogManager;
  libopenpresso::CorePtr m_core;
  std::shared_ptr<const BrewTimer> m_brewTimer;
  std::shared_ptr<db::brew_profiles::AsyncBrewProfilesRepository> m_profilesRepository;
  std::shared_ptr<db::brew_history::AsyncBrewHistoryRepository> m_historyRepository;
};

} // namespace openpressod

#endif // OPENPPRESSOD_SERVICE_IMPL_HPP