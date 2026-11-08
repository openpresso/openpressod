#ifndef ASYNC_BREW_HISTORY_REPOSITORY_HPP
#define ASYNC_BREW_HISTORY_REPOSITORY_HPP

#include "brew_log/brew_log_data.hpp"
#include "repository.hpp"
#include "utils/procedures_serial_executor.hpp"

#include <cstdint>
#include <future>
#include <string>
#include <utility>

#include <openpresso_proto/brew_log.pb.h>
#include <openpresso_proto/brew_profile.pb.h>

namespace openpressod::db::brew_history
{

class AsyncBrewHistoryRepository {
public:
  explicit AsyncBrewHistoryRepository(std::string path);
  explicit AsyncBrewHistoryRepository(BrewHistoryRepository&& repository);
  AsyncBrewHistoryRepository(const AsyncBrewHistoryRepository&) = delete;
  AsyncBrewHistoryRepository(AsyncBrewHistoryRepository&&) = delete;
  auto operator=(const AsyncBrewHistoryRepository&) = delete;
  auto operator=(AsyncBrewHistoryRepository&&) = delete;
  ~AsyncBrewHistoryRepository() = default;

  std::future<uint64_t> createEmptyLog();
  template <typename Cb>
  void createEmptyLog(Cb&& callback);

  std::future<void> completeLog(uint64_t id, CompletedBrewLog log, BrewProfile profile);
  template <typename Cb>
  void completeLog(uint64_t id, CompletedBrewLog log, BrewProfile profile, Cb&& callback);

  std::future<std::pair<CompletedBrewLog, BrewProfile>> get(uint64_t id);
  template <typename Cb>
  void get(uint64_t id, Cb&& callback);

  std::future<void> erase(uint64_t id);
  template <typename Cb>
  void erase(uint64_t id, Cb&& callback);

  std::future<BrewLogsSearchResult> find(FindBrewLogs query);
  template <typename Cb>
  void find(FindBrewLogs query, Cb&& callback);

private:
  BrewHistoryRepository m_repository;
  ProceduresSerialExecutor m_executor;
};

template <typename Cb>
inline void AsyncBrewHistoryRepository::createEmptyLog(Cb&& callback)
{
  m_executor.executeWithCallback(&BrewHistoryRepository::createEmptyLog, std::forward<Cb>(callback), &m_repository);
}

template <typename Cb>
inline void AsyncBrewHistoryRepository::completeLog(
  uint64_t id, CompletedBrewLog log, BrewProfile profile, Cb&& callback)
{
  m_executor.executeWithCallback(&BrewHistoryRepository::completeLog,
                                 std::forward<Cb>(callback),
                                 &m_repository,
                                 id,
                                 std::move(log),
                                 std::move(profile));
}

template <typename Cb>
inline void AsyncBrewHistoryRepository::get(uint64_t id, Cb&& callback)
{
  m_executor.executeWithCallback(&BrewHistoryRepository::get, std::forward<Cb>(callback), &m_repository, id);
}

template <typename Cb>
inline void AsyncBrewHistoryRepository::erase(uint64_t id, Cb&& callback)
{
  m_executor.executeWithCallback(&BrewHistoryRepository::erase, std::forward<Cb>(callback), &m_repository, id);
}

template <typename Cb>
inline void AsyncBrewHistoryRepository::find(FindBrewLogs query, Cb&& callback)
{
  m_executor.executeWithCallback(
    &BrewHistoryRepository::find, std::forward<Cb>(callback), &m_repository, std::move(query));
}

} // namespace openpressod::db::brew_history

#endif // ASYNC_BREW_HISTORY_REPOSITORY_HPP
