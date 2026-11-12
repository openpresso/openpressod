#ifndef PROTOBUF_HELPERS_HPP
#define PROTOBUF_HELPERS_HPP

#include <chrono>

#include <google/protobuf/duration.pb.h>
#include <google/protobuf/timestamp.pb.h>

namespace openpressod::protobuf_helpers
{

void toProtoDuration(std::chrono::steady_clock::duration duration, google::protobuf::Duration& result);

void toProtoTimestamp(std::chrono::system_clock::time_point timePoint, google::protobuf::Timestamp& result);

} // namespace openpressod::protobuf_helpers

#endif // PROTOBUF_HELPERS_HPP
