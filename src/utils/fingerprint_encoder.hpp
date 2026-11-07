#ifndef FINGERPRINT_ENCODER_HPP
#define FINGERPRINT_ENCODER_HPP

#include <cstdint>
#include <string>
#include <utility>
#include <xxhash.h>

namespace openpressod
{

class FingerprintEncoder {
public:
  explicit FingerprintEncoder(std::size_t capacity = 0)
  {
    m_buffer.reserve(capacity);
  }

  template <typename T>
    requires(std::is_arithmetic_v<T> && !std::same_as<T, bool>)
  void encode(const T value)
  {
    // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast)
    m_buffer.append(reinterpret_cast<const char*>(&value), sizeof(value));
    // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)
  }

  void encode(bool value)
  {
    m_buffer.push_back(value ? '\1' : '\0');
  }

  void encode(std::string_view value)
  {
    encode(static_cast<uint64_t>(value.size()));

    if (!value.empty()) {
      m_buffer.append(value.data(), value.size());
    }
  }

  template <typename T>
    requires std::is_enum_v<T>
  void encode(T value)
  {
    encode(std::to_underlying(value));
  }

  uint64_t fingerprint() const
  {
    return XXH3_64bits(m_buffer.data(), m_buffer.size());
  }

private:
  std::string m_buffer;
};

} // namespace openpressod

#endif // FINGERPRINT_ENCODER_HPP