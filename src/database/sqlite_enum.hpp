#ifndef SQLITE_ENUM_HPP
#define SQLITE_ENUM_HPP

#include <string>
#include <type_traits>

#include <sqlite_orm/sqlite_orm.h>

namespace openpressod::db
{

// Opt-in: disabled by default.
template <class E>
struct integer_enum : std::false_type {};

template <class E>
inline constexpr bool integer_enum_v = integer_enum<E>::value;

} // namespace openpressod::db

namespace sqlite_orm
{

template <class E>
struct type_printer<E, std::enable_if_t<openpressod::db::integer_enum_v<E>>> : integer_printer {};

template <class E>
struct statement_binder<E, std::enable_if_t<openpressod::db::integer_enum_v<E>>> {
  using U = std::underlying_type_t<E>;

  int bind(sqlite3_stmt* stmt, int index, E value) const
  {
    return statement_binder<U>{}.bind(stmt, index, static_cast<U>(value));
  }
};

template <class E>
struct row_extractor<E, std::enable_if_t<openpressod::db::integer_enum_v<E>>> {
  using U = std::underlying_type_t<E>;

  E extract(const char* text) const
  {
    return static_cast<E>(row_extractor<U>{}.extract(text));
  }

  E extract(sqlite3_stmt* stmt, int column) const
  {
    return static_cast<E>(row_extractor<U>{}.extract(stmt, column));
  }

  E extract(sqlite3_value* value) const
  {
    return static_cast<E>(row_extractor<U>{}.extract(value));
  }
};

template <class E>
struct field_printer<E, std::enable_if_t<openpressod::db::integer_enum_v<E>>> {
  using U = std::underlying_type_t<E>;

  std::string operator()(E value) const
  {
    return field_printer<U>{}(static_cast<U>(value));
  }
};

} // namespace sqlite_orm

#endif // SQLITE_ENUM_HPP