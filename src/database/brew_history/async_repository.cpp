#include "async_repository.hpp"

#include "brew_log/brew_log_data.hpp"
#include "repository.hpp"

#include <cstdint>
#include <future>
#include <string>
#include <utility>

#include <openpresso_proto/brew_log.pb.h>
#include <openpresso_proto/brew_profile.pb.h>

using namespace openpressod::db::brew_history;

AsyncBrewHistoryRepository::AsyncBrewHistoryRepository(std::string path)
: m_repository(std::move(path))
{
}

AsyncBrewHistoryRepository::AsyncBrewHistoryRepository(const BrewHistoryRepository& repository)
: m_repository{repository}
{
}

std::future<uint64_t> AsyncBrewHistoryRepository::createEmptyLog()
{
  return m_executor.executeWithFuture(&BrewHistoryRepository::createEmptyLog, &m_repository);
}

std::future<void> AsyncBrewHistoryRepository::completeLog(uint64_t id, CompletedBrewLog log, BrewProfile profile)
{
  return m_executor.executeWithFuture(
    &BrewHistoryRepository::completeLog, &m_repository, id, std::move(log), std::move(profile));
}

std::future<std::pair<openpressod::CompletedBrewLog, openpressod::BrewProfile>>
AsyncBrewHistoryRepository::get(uint64_t id)
{
  return m_executor.executeWithFuture(&BrewHistoryRepository::get, &m_repository, id);
}

std::future<void> AsyncBrewHistoryRepository::erase(uint64_t id)
{
  return m_executor.executeWithFuture(&BrewHistoryRepository::erase, &m_repository, id);
}

std::future<openpressod::BrewLogsSearchResult> AsyncBrewHistoryRepository::find(FindBrewLogs query)
{
  return m_executor.executeWithFuture(&BrewHistoryRepository::find, &m_repository, std::move(query));
}
