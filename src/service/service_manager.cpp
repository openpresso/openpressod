#include "service_manager.hpp"

#include "config/openpressod_config.hpp"

#include <chrono>

#include <grpcpp/security/server_credentials.h>
#include <spdlog/spdlog.h>

using namespace openpressod;

ServiceManager::ServiceManager(const std::shared_ptr<AsyncEventDispatcher>& dispatcher,
                               const libopenpresso::CorePtr& core,
                               const OpenpressodConfig& config)
: m_impl{dispatcher, core}
, m_server{buildServer(config.socketPath())}
{
}

ServiceManager::~ServiceManager()
{
  if (m_server) {
    spdlog::info("Shutting down grpc server");
    m_server->Shutdown(std::chrono::system_clock::now());
    spdlog::info("Waiting for server close");
    m_server->Wait();
  }
}

std::unique_ptr<grpc::Server> ServiceManager::buildServer(const std::filesystem::path& socketPath)
{
  grpc::ServerBuilder builder;

  builder.AddListeningPort("unix:" + socketPath.string(), grpc::InsecureServerCredentials());
  builder.RegisterService(&m_impl);
  return builder.BuildAndStart();
}
