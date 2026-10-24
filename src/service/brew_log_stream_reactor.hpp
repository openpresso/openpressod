#ifndef BREW_LOG_STREAM_REACTOR_HPP
#define BREW_LOG_STREAM_REACTOR_HPP

#include <atomic>
#include <chrono>
#include <cstddef>
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

class BrewLogStreamReactor final : public grpc::ServerWriteReactor<BrewLogRecord> {
  enum class WriteStatus : uint8_t {
    Idle,
    InProgress,
    Failed,
  };

public:
  explicit BrewLogStreamReactor(BrewLogManager* owner);

  void OnWriteDone(bool ok) override;
  void OnCancel() override;
  void OnDone() override;

  void sendBrewStart(std::chrono::steady_clock::duration samplingInterval,
                     std::chrono::system_clock::time_point startPoint);

  template <std::ranges::input_range R>
  void sendRecords(const R& records)
  {
    std::list<BrewLogRecord> recordsQueue;

    for (auto&& record : records) {
      processStepChange(record.step, recordsQueue);

      BrewLogRecord brewRecord;
      brewRecord.mutable_metrics()->set_temperature(record.temperature);
      brewRecord.mutable_metrics()->set_pressure(record.pressure);
      brewRecord.mutable_metrics()->set_weight(record.weight);
      brewRecord.mutable_metrics()->set_flowrate(record.flowRate);

      recordsQueue.push_back(std::move(brewRecord));
    }

    pushRecords(std::move(recordsQueue));
  }

private:
  void pushRecord(BrewLogRecord&& record);
  void pushRecords(std::list<BrewLogRecord>&& recordsQueue);
  void processStepChange(size_t stepIndex, std::list<BrewLogRecord>& queue);
  void worker();

private:
  size_t m_currentStep = 0;
  std::atomic<WriteStatus> m_writeStatus = WriteStatus::Idle;
  BrewLogManager* m_owner;
  std::atomic<size_t> m_queueSize = 0;
  std::mutex m_queueLock;
  std::list<BrewLogRecord> m_recordsQueue;
  std::thread m_worker;
};

} // namespace openpressod

#endif // BREW_LOG_STREAM_REACTOR_HPP
