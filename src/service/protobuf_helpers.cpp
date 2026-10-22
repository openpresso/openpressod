#include "protobuf_helpers.hpp"

#include <chrono>
#include <cstdint>

namespace openpressod::protobuf_helpers
{

void toProtoDuration(std::chrono::steady_clock::duration duration, google::protobuf::Duration& result)
{
  using namespace std::chrono;

  const auto seconds = duration_cast<std::chrono::seconds>(duration);
  const auto nanos = duration_cast<std::chrono::nanoseconds>(duration - seconds);

  result.set_seconds(seconds.count());
  result.set_nanos(static_cast<int32_t>(nanos.count()));
}

void toProtoTimestamp(std::chrono::system_clock::time_point timePoint, google::protobuf::Timestamp& result)
{
  using namespace std::chrono;

  const auto seconds = floor<std::chrono::seconds>(timePoint.time_since_epoch());
  const auto nanos = duration_cast<std::chrono::nanoseconds>(timePoint.time_since_epoch() - seconds);

  result.set_seconds(seconds.count());
  result.set_nanos(static_cast<int32_t>(nanos.count()));
}

} // namespace openpressod::protobuf_helpers
