#ifndef BREW_LOG_STREAM_REACTOR_HPP
#define BREW_LOG_STREAM_REACTOR_HPP

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <list>
#include <mutex>
#include <ranges>
#include <thread>
#include <utility>

#include <grpcpp/support/server_callback.h>
#include <openpresso_proto/openpresso.pb.h>

namespace openpressod
{

class BrewLogManager;

class BrewLogStreamReactor final : public grpc::ServerWriteReactor<BrewLog> {
  enum class WriteStatus : uint8_t {
    Idle,
    InProgress,
    Failed,
  };

public:
  explicit BrewLogStreamReactor(BrewLogManager* owner, uint32_t brewId);

  void OnWriteDone(bool ok) override;
  void OnCancel() override;
  void OnDone() override;

  uint32_t brewId() const noexcept;

  template <std::ranges::input_range R>
  void sendBrewStart(std::chrono::steady_clock::duration samplingInterval,
                     std::chrono::system_clock::time_point startPoint,
                     const R& records)
  {
    std::list<BrewLog> queue = {makeBrewStart(samplingInterval, startPoint)};
    queue.splice(queue.end(), makeRecords(records));
    pushRecords(std::move(queue));
  }

  template <typename T>
  void sendRecord(const T& recordData)
  {
    pushRecord(makeRecord(recordData));
  }

  void sendBrewStop();
  void sendLogNotFound();

  template <std::ranges::input_range R>
  void sendCompletedBrew(std::chrono::steady_clock::duration samplingInterval,
                         std::chrono::system_clock::time_point startPoint,
                         const R& records)
  {
    std::list<BrewLog> queue = {makeBrewStart(samplingInterval, startPoint)};
    queue.splice(queue.end(), makeRecords(records));
    queue.push_back(makeBrewStop());
    pushRecords(std::move(queue));
  }

private:
  void pushRecords(std::list<BrewLog>&& recordsQueue);
  void pushRecord(BrewLog record);
  void worker();

  static BrewLog makeBrewStart(std::chrono::steady_clock::duration samplingInterval,
                               std::chrono::system_clock::time_point startPoint);

  static BrewLog makeBrewStop();

  template <typename T>
  static BrewLog makeRecord(const T& recordData)
  {
    BrewLog brewRecord;
    brewRecord.mutable_record()->set_temperature(recordData.temperature);
    brewRecord.mutable_record()->set_pressure(recordData.pressure);
    brewRecord.mutable_record()->set_weight(recordData.weight);
    brewRecord.mutable_record()->set_flowrate(recordData.flowRate);
    brewRecord.mutable_record()->set_stepindex(recordData.step);
    return brewRecord;
  }

  template <std::ranges::input_range R>
  std::list<BrewLog> makeRecords(const R& records)
  {
    auto recordsView =
      records | std::views::transform([](auto&& record) { return makeRecord(record); });

    return {recordsView.begin(), recordsView.end()};
  }

private:
  uint32_t m_brewId;
  std::atomic<WriteStatus> m_writeStatus = WriteStatus::Idle;
  BrewLogManager* m_owner;
  std::mutex m_queueLock;
  std::condition_variable m_queueCv;
  bool m_cancelled = false;
  std::list<BrewLog> m_recordsQueue;
  std::thread m_worker;
};

} // namespace openpressod

#endif // BREW_LOG_STREAM_REACTOR_HPP
