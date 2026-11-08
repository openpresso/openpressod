#ifndef ASYNC_BREW_PROFILES_REPOSITORY_HPP
#define ASYNC_BREW_PROFILES_REPOSITORY_HPP

#include "repository.hpp"
#include "utils/procedures_serial_executor.hpp"

#include <cstdint>
#include <future>
#include <string>
#include <utility>

#include <openpresso_proto/brew_profile.pb.h>

namespace openpressod::db::brew_profiles
{

class AsyncBrewProfilesRepository {
public:
  explicit AsyncBrewProfilesRepository(std::string path);
  AsyncBrewProfilesRepository(const AsyncBrewProfilesRepository&) = delete;
  AsyncBrewProfilesRepository(AsyncBrewProfilesRepository&&) = delete;
  auto operator=(const AsyncBrewProfilesRepository&) = delete;
  auto operator=(AsyncBrewProfilesRepository&&) = delete;
  ~AsyncBrewProfilesRepository() = default;

  std::future<uint64_t> create(BrewProfile profile);
  template <typename Cb>
  void create(BrewProfile profile, Cb&& callback);

  std::future<void> update(uint64_t id, BrewProfile profile);
  template <typename Cb>
  void update(uint64_t id, BrewProfile profile, Cb&& callback);

  std::future<PersistentBrewProfile> get(uint64_t id);
  template <typename Cb>
  void get(uint64_t id, Cb&& callback);

  std::future<void> erase(uint64_t id);
  template <typename Cb>
  void erase(uint64_t id, Cb&& callback);

  std::future<PersistentBrewProfile> find(BrewProfile profile);
  template <typename Cb>
  void find(BrewProfile profile, Cb&& callback);

  std::future<BrewProfilesSearchResult> find(FindBrewProfiles query);
  template <typename Cb>
  void find(FindBrewProfiles query, Cb&& callback);

private:
  BrewProfilesRepository m_repository;
  ProceduresSerialExecutor m_executor;
};

template <typename Cb>
inline void AsyncBrewProfilesRepository::create(BrewProfile profile, Cb&& callback)
{
  m_executor.executeWithCallback(
    &BrewProfilesRepository::create, std::forward<Cb>(callback), &m_repository, std::move(profile));
}

template <typename Cb>
inline void AsyncBrewProfilesRepository::update(uint64_t id, BrewProfile profile, Cb&& callback)
{
  m_executor.executeWithCallback(
    &BrewProfilesRepository::update, std::forward<Cb>(callback), &m_repository, id, std::move(profile));
}

template <typename Cb>
inline void AsyncBrewProfilesRepository::get(uint64_t id, Cb&& callback)
{
  m_executor.executeWithCallback(&BrewProfilesRepository::get, std::forward<Cb>(callback), &m_repository, id);
}

template <typename Cb>
inline void AsyncBrewProfilesRepository::erase(uint64_t id, Cb&& callback)
{
  m_executor.executeWithCallback(&BrewProfilesRepository::erase, std::forward<Cb>(callback), &m_repository, id);
}

template <typename Cb>
inline void AsyncBrewProfilesRepository::find(BrewProfile profile, Cb&& callback)
{
  using FindProfile = PersistentBrewProfile (BrewProfilesRepository::*)(const BrewProfile&);
  constexpr FindProfile findProfile = &BrewProfilesRepository::find;
  m_executor.executeWithCallback(findProfile, std::forward<Cb>(callback), &m_repository, std::move(profile));
}

template <typename Cb>
inline void AsyncBrewProfilesRepository::find(FindBrewProfiles query, Cb&& callback)
{
  using FindProfiles = BrewProfilesSearchResult (BrewProfilesRepository::*)(const FindBrewProfiles&);
  constexpr FindProfiles findProfiles = &BrewProfilesRepository::find;
  m_executor.executeWithCallback(findProfiles, std::forward<Cb>(callback), &m_repository, std::move(query));
}

} // namespace openpressod::db::brew_profiles

#endif // ASYNC_BREW_PROFILES_REPOSITORY_HPP
