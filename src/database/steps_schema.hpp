#ifndef STEPS_SCHEMA_HPP
#define STEPS_SCHEMA_HPP

#include "database/database_id.hpp"
#include "database/sqlite_enum.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>

#include <openpresso_proto/brew_profile.pb.h>
#include <sqlite_orm/sqlite_orm.h>

namespace openpressod::db
{

template <>
struct integer_enum<StopConditionType> : std::true_type {};

template <>
struct integer_enum<BrewStepTargetType> : std::true_type {};

template <>
struct integer_enum<BrewStepAdvanceConditionType> : std::true_type {};

struct StepRow {
  DatabaseId profileId = 0;
  uint32_t position = 0;
  BrewStepTargetType targetType = BrewStepTargetType_UNSPECIFIED;
  float targetValue = 0;
  std::optional<BrewStepAdvanceConditionType> advanceType;
  std::optional<float> advanceValue;
};

template <typename ProfileRowT>
auto makeStepsTable()
{
  using namespace sqlite_orm;

  return make_table(
    "steps",
    make_column("profile_id", &StepRow::profileId),
    make_column("position", &StepRow::position),
    make_column("target_type", &StepRow::targetType),
    make_column("target_value", &StepRow::targetValue),
    make_column("advance_type", &StepRow::advanceType),
    make_column("advance_value", &StepRow::advanceValue),

    primary_key(&StepRow::profileId, &StepRow::position),
    foreign_key(&StepRow::profileId).references(&ProfileRowT::id).on_delete.cascade(),
    check(or_(and_(is_null(&StepRow::advanceType), is_null(&StepRow::advanceValue)),
              and_(is_not_null(&StepRow::advanceType), is_not_null(&StepRow::advanceValue)))));
}

} // namespace openpressod::db

#endif // STEPS_SCHEMA_HPP
