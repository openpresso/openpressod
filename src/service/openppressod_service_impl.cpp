#include "openppressod_service_impl.hpp"

#include "async_events_dispatcher/async_events_dispatcher.hpp"
#include "brew_history/brew_history_manager.hpp"
#include "brew_history_stream_reactor.hpp"
#include "database/brew_history/async_repository.hpp"
#include "database/brew_profiles/async_repository.hpp"
#include "database/database_id.hpp"
#include "events_stream_reactor.hpp"
#include "metrics_stream_reactor.hpp"

#include <chrono>
#include <cstdint>
#include <exception>
#include <future>
#include <memory>
#include <sqlite3.h>
#include <stdexcept>
#include <system_error>
#include <utility>

#include <google/protobuf/empty.pb.h>
#include <grpcpp/server_context.h>
#include <grpcpp/support/server_callback.h>
#include <grpcpp/support/status.h>
#include <libopenpresso/interfaces/libopenpresso_core.hpp>
#include <openpresso_proto/brew_history.pb.h>
#include <openpresso_proto/brew_profile.pb.h>
#include <openpresso_proto/events.pb.h>
#include <openpresso_proto/machine_state.pb.h>
#include <openpresso_proto/metrics.pb.h>
#include <openpresso_proto/user_settings.pb.h>
#include <spdlog/spdlog.h>
#include <sqlite_orm/sqlite_orm.h>

using namespace openpressod;

OpenpressodServiceImpl::OpenpressodServiceImpl(
  std::shared_ptr<AsyncEventDispatcher> dispatcher,
  std::shared_ptr<BrewHistoryManager> brewHistoryManager,
  libopenpresso::CorePtr core,
  std::shared_ptr<const BrewTimer> brewTimer,
  std::shared_ptr<db::brew_profiles::AsyncBrewProfilesRepository> profilesRepository,
  std::shared_ptr<db::brew_history::AsyncBrewHistoryRepository> historyRepository)
: m_dispatcher{std::move(dispatcher)}
, m_brewHistoryManager{std::move(brewHistoryManager)}
, m_core{std::move(core)}
, m_brewTimer{std::move(brewTimer)}
, m_profilesRepository{std::move(profilesRepository)}
, m_historyRepository{std::move(historyRepository)}
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

grpc::ServerUnaryReactor* OpenpressodServiceImpl::getActiveBrewProfile(
  grpc::CallbackServerContext* context,
  [[maybe_unused]] const google::protobuf::Empty* request,
  ActiveBrewProfile* response)
{
  auto* reactor = context->DefaultReactor();
  auto callback = [reactor, response](std::future<ActiveBrewProfile> result) {
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

grpc::ServerUnaryReactor* OpenpressodServiceImpl::setActiveBrewProfile(
  grpc::CallbackServerContext* context,
  const ActivateBrewProfile* request,
  [[maybe_unused]] google::protobuf::Empty* response)
{
  auto* reactor = context->DefaultReactor();
  if (request->has_profile()) {
    m_dispatcher->setBrewProfile(request->profile(), makeVoidCallback(reactor));
  }
  else if (request->has_persistentid()) {
    m_profilesRepository->get(request->persistentid(), [reactor, this](std::future<PersistentBrewProfile> result) {
      try {
        m_dispatcher->setBrewProfile(result.get().profile(), makeVoidCallback(reactor));
      }
      catch (const std::system_error& error) {
        reactor->Finish(dbError(error));
      }
    });
  }
  else {
    reactor->Finish(grpc::Status{grpc::StatusCode::INVALID_ARGUMENT, "Message has no valid profile"});
  }

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
  auto callback = [reactor, response](std::future<BrewState> result) {
    try {
      response->CopyFrom(result.get());
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
    auto reactor =
      std::make_unique<MetricsStreamReactor>(m_core, m_brewTimer, updateRate, request->pidsource());
    return reactor.release();
  }
  catch (const std::runtime_error& e) {
    spdlog::error(e.what());
    return nullptr;
  }
}

grpc::ServerWriteReactor<BrewHistoryStream>* OpenpressodServiceImpl::getBrewHistoryEntry(
  [[maybe_unused]] grpc::CallbackServerContext* context, const PersistentId* request)
{
  auto reactor = std::make_unique<BrewHistoryStreamReactor>(m_brewHistoryManager.get(), request->id());
  auto* raw = reactor.get();
  m_brewHistoryManager->addStreamReactor(std::move(reactor));
  return raw;
}

grpc::ServerUnaryReactor* OpenpressodServiceImpl::getPersistentBrewProfile(
  grpc::CallbackServerContext* context, const PersistentId* request, PersistentBrewProfile* response)
{
  auto* reactor = context->DefaultReactor();
  m_profilesRepository->get(request->id(), [reactor, response](std::future<PersistentBrewProfile> result) {
    try {
      *response = result.get();
      reactor->Finish(grpc::Status::OK);
    }
    catch (const std::system_error& error) {
      reactor->Finish(dbError(error));
    }
  });
  return reactor;
}

grpc::ServerUnaryReactor* OpenpressodServiceImpl::addPersistentBrewProfile(
  grpc::CallbackServerContext* context, const BrewProfile* request, PersistentId* response)
{
  auto* reactor = context->DefaultReactor();
  m_profilesRepository->create(*request, [reactor, response](std::future<DatabaseId> result) {
    try {
      response->set_id(result.get());
      reactor->Finish(grpc::Status::OK);
    }
    catch (const std::system_error& error) {
      reactor->Finish(dbError(error));
    }
  });
  return reactor;
}

grpc::ServerUnaryReactor* OpenpressodServiceImpl::updatePersistentBrewProfile(
  grpc::CallbackServerContext* context,
  const UpdatePersistentProfile* request,
  [[maybe_unused]] google::protobuf::Empty* response)
{
  auto* reactor = context->DefaultReactor();
  m_profilesRepository->update(request->id(), request->profile(), [reactor](std::future<void> result) {
    try {
      result.get();
      reactor->Finish(grpc::Status::OK);
    }
    catch (const std::system_error& error) {
      reactor->Finish(dbError(error));
    }
  });
  return reactor;
}

grpc::ServerUnaryReactor* OpenpressodServiceImpl::deletePersistentBrewProfile(
  grpc::CallbackServerContext* context,
  const PersistentId* request,
  [[maybe_unused]] google::protobuf::Empty* response)
{
  auto* reactor = context->DefaultReactor();
  m_profilesRepository->erase(request->id(), [reactor](std::future<void> result) {
    try {
      result.get();
      reactor->Finish(grpc::Status::OK);
    }
    catch (const std::system_error& error) {
      reactor->Finish(dbError(error));
    }
  });
  return reactor;
}

grpc::ServerUnaryReactor* OpenpressodServiceImpl::findPersistentBrewProfiles(
  grpc::CallbackServerContext* context, const FindBrewProfiles* request, BrewProfilesSearchResult* response)
{
  auto* reactor = context->DefaultReactor();
  m_profilesRepository->find(*request, [reactor, response](std::future<BrewProfilesSearchResult> result) {
    try {
      *response = result.get();
      reactor->Finish(grpc::Status::OK);
    }
    catch (const std::system_error& error) {
      reactor->Finish(dbError(error));
    }
  });
  return reactor;
}

grpc::ServerUnaryReactor* OpenpressodServiceImpl::findBrewHistory(grpc::CallbackServerContext* context,
                                                                  const FindBrewHistory* request,
                                                                  BrewHistorySearchResult* response)
{
  auto* reactor = context->DefaultReactor();
  m_historyRepository->find(*request, [reactor, response](std::future<BrewHistorySearchResult> result) {
    try {
      *response = result.get();
      reactor->Finish(grpc::Status::OK);
    }
    catch (const std::system_error& error) {
      reactor->Finish(dbError(error));
    }
  });
  return reactor;
}

grpc::ServerUnaryReactor* OpenpressodServiceImpl::removeBrewHistoryEntry(
  grpc::CallbackServerContext* context,
  const PersistentId* request,
  [[maybe_unused]] google::protobuf::Empty* response)
{
  auto* reactor = context->DefaultReactor();
  m_historyRepository->erase(request->id(), [reactor](std::future<void> result) {
    try {
      result.get();
      reactor->Finish(grpc::Status::OK);
    }
    catch (const std::system_error& error) {
      reactor->Finish(dbError(error));
    }
  });
  return reactor;
}

grpc::Status OpenpressodServiceImpl::dbError(const std::system_error& error)
{
  if (error.code() == sqlite_orm::make_error_code(sqlite_orm::orm_error_code::not_found)) {
    return grpc::Status{grpc::StatusCode::NOT_FOUND, error.what()};
  }

  if (error.code() == sqlite_orm::make_error_code(sqlite_orm::sqlite_errc(SQLITE_CONSTRAINT_UNIQUE))) {
    return grpc::Status{grpc::StatusCode::ALREADY_EXISTS, "Database unique constraint violated"};
  }

  return grpc::Status{grpc::StatusCode::INTERNAL, error.what()};
}
