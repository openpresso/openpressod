#ifndef BREW_PROFILE_SCHEMA_HPP
#define BREW_PROFILE_SCHEMA_HPP

#include "database/sqlite_enum.hpp"
#include "database/steps_schema.hpp"

#include <cstdint>
#include <optional>
#include <string>

#include <openpresso_proto/brew_profile.pb.h>
#include <sqlite_orm/sqlite_orm.h>

namespace openpressod::db
{

template <>
struct integer_enum<StopConditionType> : std::true_type {};

namespace brew_profiles
{

struct ProfileRow {
  uint64_t id = 0;
  std::string name;
  int32_t temperature = 0;
  std::optional<StopConditionType> stopType;
  std::optional<float> stopValue;
  uint64_t created = 0;
  uint64_t modified = 0;
  uint64_t fingerprint = 0;
};

inline auto makeProfilesTable()
{
  using namespace sqlite_orm;

  return make_table(
    "profiles",
    make_column("id", &ProfileRow::id, primary_key().autoincrement()),
    make_column("name", &ProfileRow::name, unique(), check(length(&ProfileRow::name) > 0)),
    make_column("temperature", &ProfileRow::temperature),
    make_column("stop_type", &ProfileRow::stopType),
    make_column("stop_value", &ProfileRow::stopValue),
    make_column("created_at", &ProfileRow::created),
    make_column("modified_at", &ProfileRow::modified),
    make_column("fingerprint", &ProfileRow::fingerprint, unique()),

    check(or_(and_(is_null(&ProfileRow::stopType), is_null(&ProfileRow::stopValue)),
              and_(is_not_null(&ProfileRow::stopType), is_not_null(&ProfileRow::stopValue)))));
}

inline auto makeStorage(std::string filename)
{
  using namespace sqlite_orm;

  // sqlite_orm synchronizes schema objects in reverse declaration order.
  // Create parent tables before child tables, and tables before their indexes.
  return make_storage(std::move(filename),
                      make_index("ix_profiles_name_like", indexed_column(&ProfileRow::name).collate("NOCASE")),
                      make_index("ix_profiles_created_at", &ProfileRow::created),
                      make_index("ix_profiles_modified_at", &ProfileRow::modified),
                      make_index("ix_profiles_fingerprint", &ProfileRow::fingerprint),
                      makeStepsTable<ProfileRow>(),
                      makeProfilesTable());
}

using Storage = decltype(makeStorage({}));

} // namespace brew_profiles

} // namespace openpressod::db

#endif // BREW_PROFILE_SCHEMA_HPP