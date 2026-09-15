#ifndef EVENTS_STREAM_REACTOR_HPP
#define EVENTS_STREAM_REACTOR_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <list>
#include <mutex>
#include <thread>

#include <grpcpp/support/server_callback.h>
#include <openpresso_proto/openpresso.pb.h>

namespace openpressod
{

class AsyncEventDispatcher;

class EventsStreamReactor final : public grpc::ServerWriteReactor<Event> {
  enum class WriteStatus : uint8_t {
    Idle,
    InProgress,
    Failed,
  };

public:
  EventsStreamReactor(AsyncEventDispatcher* owner);

  void OnWriteDone(bool ok) override;
  void OnCancel() override;
  void OnDone() override;

  void notifyChanged(const PowerState& powerState);
  void notifyChanged(const BrewState& brewState);
  void notifyChanged(const SteamModeState& steamModeState);
  void notifyChanged(const BrewProfile& brewProfile);
  void notifyChanged(const UserSettings& userSettings);
  void notifyChanged(const BrewProgress& brewProgress);

private:
  void pushEvent(Event&& event);
  void worker();

private:
  std::atomic<WriteStatus> m_writeStatus = WriteStatus::Idle;
  AsyncEventDispatcher* m_owner;
  std::atomic<size_t> m_queueSize = 0;
  std::mutex m_queueLock;
  std::list<Event> m_eventsQueue;
  std::thread m_worker;
};

} // namespace openpressod

#endif // EVENTS_STREAM_REACTOR_HPP