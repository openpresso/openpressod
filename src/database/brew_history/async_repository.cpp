#include "async_repository.hpp"

#include "brew_history/brew_history_data.hpp"
#include "database/database_id.hpp"
#include "repository.hpp"

#include <future>
#include <string>
#include <utility>

#include <openpresso_proto/brew_history.pb.h>

using namespace openpressod::db::brew_history;

AsyncBrewHistoryRepository::AsyncBrewHistoryRepository(std::string path)
: m_repository(std::move(path))
{
}

AsyncBrewHistoryRepository::AsyncBrewHistoryRepository(const BrewHistoryRepository& repository)
: m_repository{repository}
{
}

std::future<openpressod::DatabaseId> AsyncBrewHistoryRepository::createEmptyEntry()
{
  return m_executor.executeWithFuture(&BrewHistoryRepository::createEmptyEntry, &m_repository);
}

std::future<void> AsyncBrewHistoryRepository::completeEntry(DatabaseId id, CompletedBrewHistoryEntry entry)
{
  return m_executor.executeWithFuture(
    &BrewHistoryRepository::completeEntry, &m_repository, id, std::move(entry));
}

std::future<openpressod::CompletedBrewHistoryEntry> AsyncBrewHistoryRepository::get(DatabaseId id)
{
  return m_executor.executeWithFuture(&BrewHistoryRepository::get, &m_repository, id);
}

std::future<void> AsyncBrewHistoryRepository::erase(DatabaseId id)
{
  return m_executor.executeWithFuture(&BrewHistoryRepository::erase, &m_repository, id);
}

std::future<openpressod::BrewHistorySearchResult> AsyncBrewHistoryRepository::find(FindBrewHistory query)
{
  return m_executor.executeWithFuture(&BrewHistoryRepository::find, &m_repository, std::move(query));
}
