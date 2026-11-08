#include "async_repository.hpp"

#include "repository.hpp"

#include <cstdint>
#include <future>
#include <string>
#include <utility>

#include <openpresso_proto/brew_profile.pb.h>

using namespace openpressod::db::brew_profiles;

AsyncBrewProfilesRepository::AsyncBrewProfilesRepository(std::string path)
: m_repository(std::move(path))
{
}

std::future<uint64_t> AsyncBrewProfilesRepository::create(BrewProfile profile)
{
  return m_executor.executeWithFuture(&BrewProfilesRepository::create, &m_repository, std::move(profile));
}

std::future<void> AsyncBrewProfilesRepository::update(uint64_t id, BrewProfile profile)
{
  return m_executor.executeWithFuture(&BrewProfilesRepository::update, &m_repository, id, std::move(profile));
}

std::future<openpressod::PersistentBrewProfile> AsyncBrewProfilesRepository::get(uint64_t id)
{
  return m_executor.executeWithFuture(&BrewProfilesRepository::get, &m_repository, id);
}

std::future<void> AsyncBrewProfilesRepository::erase(uint64_t id)
{
  return m_executor.executeWithFuture(&BrewProfilesRepository::erase, &m_repository, id);
}

std::future<openpressod::PersistentBrewProfile> AsyncBrewProfilesRepository::find(BrewProfile profile)
{
  using FindProfile = PersistentBrewProfile (BrewProfilesRepository::*)(const BrewProfile&);
  constexpr FindProfile findProfile = &BrewProfilesRepository::find;
  return m_executor.executeWithFuture(findProfile, &m_repository, std::move(profile));
}

std::future<openpressod::BrewProfilesSearchResult> AsyncBrewProfilesRepository::find(FindBrewProfiles query)
{
  using FindProfiles = BrewProfilesSearchResult (BrewProfilesRepository::*)(const FindBrewProfiles&);
  constexpr FindProfiles findProfiles = &BrewProfilesRepository::find;
  return m_executor.executeWithFuture(findProfiles, &m_repository, std::move(query));
}
