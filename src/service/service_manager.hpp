#ifndef SERVICE_MANAGER_HPP
#define SERVICE_MANAGER_HPP

#include "async_events_dispatcher/async_events_dispatcher.hpp"
#include "openppressod_service_impl.hpp"
#include "state_manager/brew_timer.hpp"

#include <memory>

#include <grpcpp/grpcpp.h>
#include <libopenpresso/interfaces/libopenpresso_core.hpp>

namespace openpressod
{

class OpenpressodConfig;
class AsyncEventDispatcher;
class BrewTimer;
class BrewLogManager;

class ServiceManager {
public:
  ServiceManager(std::shared_ptr<AsyncEventDispatcher> dispatcher,
                 std::shared_ptr<BrewLogManager> brewLogManager,
                 libopenpresso::CorePtr core,
                 std::shared_ptr<const BrewTimer> brewTimer,
                 std::shared_ptr<db::brew_profiles::AsyncBrewProfilesRepository> profilesRepository,
                 std::shared_ptr<db::brew_history::AsyncBrewHistoryRepository> historyRepository,
                 const OpenpressodConfig& config);
  ServiceManager(const ServiceManager&) = delete;
  ServiceManager(ServiceManager&&) = delete;
  auto operator=(const ServiceManager&) = delete;
  auto operator=(ServiceManager&&) = delete;
  ~ServiceManager();

private:
  std::unique_ptr<grpc::Server> buildServer(const OpenpressodConfig& config);

private:
  OpenpressodServiceImpl m_impl;
  std::unique_ptr<grpc::Server> m_server;
};

} // namespace openpressod

#endif // SERVICE_MANAGER_HPP