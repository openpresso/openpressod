#include "service_manager.hpp"

#include "config/openpressod_config.hpp"
#include "database/brew_history/async_repository.hpp"
#include "database/brew_profiles/async_repository.hpp"

#include <chrono>
#include <filesystem>
#include <memory>
#include <utility>

#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server_builder.h>
#include <libopenpresso/interfaces/libopenpresso_core.hpp>
#include <spdlog/spdlog.h>

using namespace openpressod;

ServiceManager::ServiceManager(std::shared_ptr<AsyncEventDispatcher> dispatcher,
                               std::shared_ptr<BrewHistoryManager> brewHistoryManager,
                               libopenpresso::CorePtr core,
                               std::shared_ptr<const BrewTimer> brewTimer,
                               std::shared_ptr<db::brew_profiles::AsyncBrewProfilesRepository> profilesRepository,
                               std::shared_ptr<db::brew_history::AsyncBrewHistoryRepository> historyRepository,
                               const OpenpressodConfig& config)
: m_impl{std::move(dispatcher),
         std::move(brewHistoryManager),
         std::move(core),
         std::move(brewTimer),
         std::move(profilesRepository),
         std::move(historyRepository)}
, m_server{buildServer(config)}
{
  spdlog::debug("Openpresso service created");
}

ServiceManager::~ServiceManager()
{
  if (m_server) {
    spdlog::debug("Shutting down grpc server");
    m_server->Shutdown(std::chrono::system_clock::now());
    spdlog::debug("Waiting for server close");
    m_server->Wait();
  }
}

std::unique_ptr<grpc::Server> ServiceManager::buildServer(const OpenpressodConfig& config)
{
  grpc::ServerBuilder builder;

  builder.AddListeningPort("unix:" + config.socketPath().string(), grpc::InsecureServerCredentials());
  if (config.tcpEnable()) {
    builder.AddListeningPort(config.tcpBindAddress(), grpc::InsecureServerCredentials());
  }
  builder.RegisterService(&m_impl);
  return builder.BuildAndStart();
}
