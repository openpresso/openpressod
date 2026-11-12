#ifndef ASYNC_BREW_HISTORY_REPOSITORY_HPP
#define ASYNC_BREW_HISTORY_REPOSITORY_HPP

#include "brew_history/brew_history_data.hpp"
#include "database/database_id.hpp"
#include "repository.hpp"
#include "utils/procedures_serial_executor.hpp"

#include <future>
#include <string>
#include <utility>

#include <openpresso_proto/brew_history.pb.h>

namespace openpressod::db::brew_history
{

class AsyncBrewHistoryRepository {
public:
  explicit AsyncBrewHistoryRepository(std::string path);
  explicit AsyncBrewHistoryRepository(const BrewHistoryRepository& repository);
  AsyncBrewHistoryRepository(const AsyncBrewHistoryRepository&) = delete;
  AsyncBrewHistoryRepository(AsyncBrewHistoryRepository&&) = delete;
  auto operator=(const AsyncBrewHistoryRepository&) = delete;
  auto operator=(AsyncBrewHistoryRepository&&) = delete;
  ~AsyncBrewHistoryRepository() = default;

  std::future<DatabaseId> createEmptyEntry();
  template <typename Cb>
  void createEmptyEntry(Cb&& callback);

  std::future<void> completeEntry(DatabaseId id, CompletedBrewHistoryEntry entry);
  template <typename Cb>
  void completeEntry(DatabaseId id, CompletedBrewHistoryEntry entry, Cb&& callback);

  std::future<CompletedBrewHistoryEntry> get(DatabaseId id);
  template <typename Cb>
  void get(DatabaseId id, Cb&& callback);

  std::future<void> erase(DatabaseId id);
  template <typename Cb>
  void erase(DatabaseId id, Cb&& callback);

  std::future<BrewHistorySearchResult> find(FindBrewHistory query);
  template <typename Cb>
  void find(FindBrewHistory query, Cb&& callback);

private:
  BrewHistoryRepository m_repository;
  ProceduresSerialExecutor m_executor;
};

template <typename Cb>
inline void AsyncBrewHistoryRepository::createEmptyEntry(Cb&& callback)
{
  m_executor.executeWithCallback(&BrewHistoryRepository::createEmptyEntry,
                                 std::forward<Cb>(callback),
                                 &m_repository);
}

template <typename Cb>
inline void AsyncBrewHistoryRepository::completeEntry(DatabaseId id, CompletedBrewHistoryEntry entry, Cb&& callback)
{
  m_executor.executeWithCallback(
    &BrewHistoryRepository::completeEntry, std::forward<Cb>(callback), &m_repository, id, std::move(entry));
}

template <typename Cb>
inline void AsyncBrewHistoryRepository::get(DatabaseId id, Cb&& callback)
{
  m_executor.executeWithCallback(&BrewHistoryRepository::get, std::forward<Cb>(callback), &m_repository, id);
}

template <typename Cb>
inline void AsyncBrewHistoryRepository::erase(DatabaseId id, Cb&& callback)
{
  m_executor.executeWithCallback(&BrewHistoryRepository::erase, std::forward<Cb>(callback), &m_repository, id);
}

template <typename Cb>
inline void AsyncBrewHistoryRepository::find(FindBrewHistory query, Cb&& callback)
{
  m_executor.executeWithCallback(
    &BrewHistoryRepository::find, std::forward<Cb>(callback), &m_repository, std::move(query));
}

} // namespace openpressod::db::brew_history

#endif // ASYNC_BREW_HISTORY_REPOSITORY_HPP
