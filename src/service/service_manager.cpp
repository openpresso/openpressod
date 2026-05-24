#include "service_manager.hpp"

#include "config/openpressod_config.hpp"

#include <chrono>
#include <filesystem>
#include <memory>

#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server_builder.h>
#include <libopenpresso/interfaces/libopenpresso_core.hpp>
#include <spdlog/spdlog.h>

using namespace openpressod;

ServiceManager::ServiceManager(const std::shared_ptr<AsyncEventDispatcher>& dispatcher,
                               const libopenpresso::CorePtr& core,
                               const OpenpressodConfig& config)
: m_impl{dispatcher, core}
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
