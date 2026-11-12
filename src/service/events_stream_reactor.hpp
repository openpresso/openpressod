#ifndef EVENTS_STREAM_REACTOR_HPP
#define EVENTS_STREAM_REACTOR_HPP

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <list>
#include <mutex>
#include <thread>

#include <grpcpp/support/server_callback.h>
#include <openpresso_proto/brew_profile.pb.h>
#include <openpresso_proto/events.pb.h>
#include <openpresso_proto/machine_state.pb.h>
#include <openpresso_proto/user_settings.pb.h>

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
  explicit EventsStreamReactor(AsyncEventDispatcher* owner);

  void OnWriteDone(bool ok) override;
  void OnCancel() override;
  void OnDone() override;

  void notifyChanged(const PowerState& powerState);
  void notifyChanged(const BrewState& brewState);
  void notifyChanged(const SteamModeState& steamModeState);
  void notifyChanged(const ActiveBrewProfile& brewProfile);
  void notifyChanged(const UserSettings& userSettings);
  void notifyChanged(const BrewProgress& brewProgress);
  void notifyChanged(const HeaterState& heaterState);

private:
  void pushEvent(Event&& event);
  void worker();

private:
  std::atomic<WriteStatus> m_writeStatus = WriteStatus::Idle;
  AsyncEventDispatcher* m_owner;
  std::mutex m_queueLock;
  std::condition_variable m_queueCv;
  bool m_cancelled = false;
  std::list<Event> m_eventsQueue;
  std::thread m_worker;
};

} // namespace openpressod

#endif // EVENTS_STREAM_REACTOR_HPP
