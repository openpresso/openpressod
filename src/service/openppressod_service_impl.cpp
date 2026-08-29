#include "openppressod_service_impl.hpp"

#include "async_events_dispatcher/async_events_dispatcher.hpp"
#include "events_stream_reactor.hpp"
#include "metrics_stream_reactor.hpp"

#include <chrono>
#include <exception>
#include <future>
#include <memory>
#include <stdexcept>
#include <utility>

#include <google/protobuf/empty.pb.h>
#include <grpcpp/server_context.h>
#include <grpcpp/support/server_callback.h>
#include <grpcpp/support/status.h>
#include <libopenpresso/interfaces/libopenpresso_core.hpp>
#include <openpresso_proto/openpresso.pb.h>
#include <spdlog/spdlog.h>

using namespace openpressod;

OpenpressodServiceImpl::OpenpressodServiceImpl(const std::shared_ptr<AsyncEventDispatcher>& dispatcher,
                                               libopenpresso::CorePtr core)
: m_dispatcher{dispatcher}
, m_core{std::move(core)}
{
}

auto OpenpressodServiceImpl::makeVoidCallback(grpc::ServerUnaryReactor* reactor)
{
  return [reactor](std::future<void> result) {
    try {
      result.get();
      reactor->Finish(grpc::Status::OK);
    }
    catch (const std::exception& e) {
      reactor->Finish(grpc::Status{grpc::StatusCode::ABORTED, e.what()});
    }
  };
}

grpc::ServerUnaryReactor* OpenpressodServiceImpl::resetScales(
  grpc::CallbackServerContext* context,
  [[maybe_unused]] const google::protobuf::Empty* request,
  [[maybe_unused]] google::protobuf::Empty* response)
{
  auto* reactor = context->DefaultReactor();
  m_dispatcher->resetWeight(makeVoidCallback(reactor));
  return reactor;
}

grpc::ServerUnaryReactor* OpenpressodServiceImpl::getUserSettings(
  grpc::CallbackServerContext* context,
  [[maybe_unused]] const google::protobuf::Empty* request,
  UserSettings* response)
{
  auto* reactor = context->DefaultReactor();
  auto callback = [reactor, response](std::future<const UserSettings&> result) {
    try {
      response->CopyFrom(result.get());
      reactor->Finish(grpc::Status::OK);
    }
    catch (const std::exception& e) {
      reactor->Finish(grpc::Status{grpc::StatusCode::UNKNOWN, e.what()});
    }
  };
  m_dispatcher->getUserSettings(std::move(callback));
  return reactor;
}

grpc::ServerUnaryReactor* OpenpressodServiceImpl::setUserSettings(grpc::CallbackServerContext* context,
                                                                  const UserSettings* request,
                                                                  [[maybe_unused]] google::protobuf::Empty* response)
{
  auto* reactor = context->DefaultReactor();
  m_dispatcher->setUserSettings(request, makeVoidCallback(reactor));
  return reactor;
}

grpc::ServerUnaryReactor* OpenpressodServiceImpl::getBrewProfile(
  grpc::CallbackServerContext* context,
  [[maybe_unused]] const google::protobuf::Empty* request,
  BrewProfile* response)
{
  auto* reactor = context->DefaultReactor();
  auto callback = [reactor, response](std::future<const BrewProfile&> result) {
    try {
      response->CopyFrom(result.get());
      reactor->Finish(grpc::Status::OK);
    }
    catch (const std::exception& e) {
      reactor->Finish(grpc::Status{grpc::StatusCode::UNKNOWN, e.what()});
    }
  };
  m_dispatcher->getBrewProfile(std::move(callback));
  return reactor;
}

grpc::ServerUnaryReactor* OpenpressodServiceImpl::setBrewProfile(grpc::CallbackServerContext* context,
                                                                 const BrewProfile* request,
                                                                 [[maybe_unused]] google::protobuf::Empty* response)
{
  auto* reactor = context->DefaultReactor();
  m_dispatcher->setBrewProfile(request, makeVoidCallback(reactor));
  return reactor;
}

grpc::ServerUnaryReactor* OpenpressodServiceImpl::setPowerState(grpc::CallbackServerContext* context,
                                                                const PowerState* request,
                                                                [[maybe_unused]] google::protobuf::Empty* response)
{
  auto* reactor = context->DefaultReactor();
  m_dispatcher->setPowerState(request->value(), makeVoidCallback(reactor));
  return reactor;
}

grpc::ServerUnaryReactor* OpenpressodServiceImpl::getPowerState(
  grpc::CallbackServerContext* context,
  [[maybe_unused]] const google::protobuf::Empty* request,
  PowerState* response)
{
  auto* reactor = context->DefaultReactor();
  auto callback = [reactor, response](std::future<bool> result) {
    try {
      response->set_value(result.get());
      reactor->Finish(grpc::Status::OK);
    }
    catch (const std::exception& e) {
      reactor->Finish(grpc::Status{grpc::StatusCode::UNKNOWN, e.what()});
    }
  };
  m_dispatcher->getPowerState(std::move(callback));
  return reactor;
}

grpc::ServerUnaryReactor* OpenpressodServiceImpl::startBrew(
  grpc::CallbackServerContext* context,
  [[maybe_unused]] const google::protobuf::Empty* request,
  [[maybe_unused]] google::protobuf::Empty* response)
{
  auto* reactor = context->DefaultReactor();
  m_dispatcher->startBrew(makeVoidCallback(reactor));
  return reactor;
}

grpc::ServerUnaryReactor* OpenpressodServiceImpl::stopBrew(
  grpc::CallbackServerContext* context,
  [[maybe_unused]] const google::protobuf::Empty* request,
  [[maybe_unused]] google::protobuf::Empty* response)
{
  auto* reactor = context->DefaultReactor();
  m_dispatcher->stopBrew(makeVoidCallback(reactor));
  return reactor;
}

grpc::ServerUnaryReactor* OpenpressodServiceImpl::getBrewState(
  grpc::CallbackServerContext* context, [[maybe_unused]] const google::protobuf::Empty* request, BrewState* response)
{
  auto* reactor = context->DefaultReactor();
  auto callback = [reactor, response](std::future<bool> result) {
    try {
      response->set_value(result.get());
      reactor->Finish(grpc::Status::OK);
    }
    catch (const std::exception& e) {
      reactor->Finish(grpc::Status{grpc::StatusCode::UNKNOWN, e.what()});
    }
  };
  m_dispatcher->getBrewState(std::move(callback));
  return reactor;
}

grpc::ServerUnaryReactor* OpenpressodServiceImpl::setSteamModeState(
  grpc::CallbackServerContext* context,
  const SteamModeState* request,
  [[maybe_unused]] google::protobuf::Empty* response)
{
  auto* reactor = context->DefaultReactor();
  m_dispatcher->setSteamModeState(request->isactive(), makeVoidCallback(reactor));
  return reactor;
}

grpc::ServerUnaryReactor* OpenpressodServiceImpl::getSteamModeState(
  grpc::CallbackServerContext* context,
  [[maybe_unused]] const google::protobuf::Empty* request,
  SteamModeState* response)
{
  auto* reactor = context->DefaultReactor();
  auto callback = [reactor, response](std::future<bool> result) {
    try {
      response->set_isactive(result.get());
      reactor->Finish(grpc::Status::OK);
    }
    catch (const std::exception& e) {
      reactor->Finish(grpc::Status{grpc::StatusCode::UNKNOWN, e.what()});
    }
  };
  m_dispatcher->getSteamModeState(std::move(callback));
  return reactor;
}

grpc::ServerWriteReactor<Event>* OpenpressodServiceImpl::events(
  [[maybe_unused]] grpc::CallbackServerContext* context,
  [[maybe_unused]] const google::protobuf::Empty* request)
{
  auto reactor = std::make_unique<EventsStreamReactor>(m_dispatcher.get());
  auto* raw = reactor.get();
  m_dispatcher->addEventsStreamReactor(std::move(reactor));
  return raw;
}

grpc::ServerWriteReactor<Metrics>* OpenpressodServiceImpl::metrics(
  [[maybe_unused]] grpc::CallbackServerContext* context, const MetricsRequest* request)
{
  auto updateRate = std::chrono::seconds{request->updaterate().seconds()} +
                    std::chrono::nanoseconds{request->updaterate().nanos()};

  try {
    auto reactor = std::make_unique<MetricsStreamReactor>(m_core, updateRate, request->pidsource());
    return reactor.release();
  }
  catch (const std::runtime_error& e) {
    spdlog::error(e.what());
    return nullptr;
  }
}
