#ifndef BREW_HISTORY_STREAM_REACTOR_HPP
#define BREW_HISTORY_STREAM_REACTOR_HPP

#include "database/database_id.hpp"

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
#include <openpresso_proto/brew_history.pb.h>
#include <openpresso_proto/brew_profile.pb.h>

namespace openpressod
{

class BrewHistoryManager;

class BrewHistoryStreamReactor final : public grpc::ServerWriteReactor<BrewHistoryStream> {
  enum class WriteStatus : uint8_t {
    Idle,
    InProgress,
    Failed,
  };

public:
  explicit BrewHistoryStreamReactor(BrewHistoryManager* owner, DatabaseId brewId);

  void OnWriteDone(bool ok) override;
  void OnCancel() override;
  void OnDone() override;

  DatabaseId brewId() const noexcept;

  template <std::ranges::input_range R>
  void sendBrewStart(std::chrono::steady_clock::duration samplingInterval,
                     std::chrono::system_clock::time_point startPoint,
                     BrewProfile profile,
                     const R& samples)
  {
    std::list<BrewHistoryStream> queue = {makeBrewStart(samplingInterval, startPoint, std::move(profile))};
    queue.splice(queue.end(), makeSamples(samples));
    pushMessages(std::move(queue));
  }

  template <typename T>
  void sendSample(const T& sampleData)
  {
    pushMessage(makeSample(sampleData));
  }

  void sendBrewStop();
  void sendEntryNotFound();

  template <std::ranges::input_range R>
  void sendCompletedBrew(std::chrono::steady_clock::duration samplingInterval,
                         std::chrono::system_clock::time_point startPoint,
                         BrewProfile profile,
                         const R& samples)
  {
    std::list<BrewHistoryStream> queue = {makeBrewStart(samplingInterval, startPoint, std::move(profile))};
    queue.splice(queue.end(), makeSamples(samples));
    queue.push_back(makeBrewStop());
    pushMessages(std::move(queue));
  }

private:
  void pushMessages(std::list<BrewHistoryStream>&& messagesQueue);
  void pushMessage(BrewHistoryStream message);
  void worker();

  static BrewHistoryStream makeBrewStart(std::chrono::steady_clock::duration samplingInterval,
                                         std::chrono::system_clock::time_point startPoint,
                                         BrewProfile profile);

  static BrewHistoryStream makeBrewStop();

  template <typename T>
  static BrewHistoryStream makeSample(const T& sampleData)
  {
    BrewHistoryStream message;
    message.mutable_sample()->set_pressure(sampleData.pressure);
    message.mutable_sample()->set_weight(sampleData.weight);
    message.mutable_sample()->set_flowrate(sampleData.flowRate);
    message.mutable_sample()->set_stepindex(sampleData.step);
    return message;
  }

  template <std::ranges::input_range R>
  std::list<BrewHistoryStream> makeSamples(const R& samples)
  {
    auto samplesView =
      samples | std::views::transform([](auto&& sample) { return makeSample(sample); });

    return {samplesView.begin(), samplesView.end()};
  }

private:
  DatabaseId m_brewId;
  std::atomic<WriteStatus> m_writeStatus = WriteStatus::Idle;
  BrewHistoryManager* m_owner;
  std::mutex m_queueLock;
  std::condition_variable m_queueCv;
  bool m_cancelled = false;
  bool m_notFound = false;
  std::list<BrewHistoryStream> m_messagesQueue;
  std::thread m_worker;
};

} // namespace openpressod

#endif // BREW_HISTORY_STREAM_REACTOR_HPP
