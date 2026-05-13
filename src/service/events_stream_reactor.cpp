#include "events_stream_reactor.hpp"

#include "async_events_dispatcher/async_events_dispatcher.hpp"

#include <atomic>
#include <list>
#include <mutex>
#include <utility>

#include <openpresso_proto/openpresso.pb.h>

using namespace openpressod;

EventsStreamReactor::EventsStreamReactor(AsyncEventDispatcher* owner)
: m_owner{owner}
, m_worker{&EventsStreamReactor::worker, this}
{
}

void EventsStreamReactor::OnWriteDone(bool ok)
{
  m_writeStatus.store(ok ? WriteStatus::Idle : WriteStatus::Failed, std::memory_order_release);
  m_writeStatus.notify_one();
}

void EventsStreamReactor::OnCancel()
{
  m_queueSize.fetch_add(1, std::memory_order_release);
  m_queueSize.notify_one();
}

void EventsStreamReactor::OnDone()
{
  m_worker.join();
  m_owner->releaseEventsStreamReactor(this);
}

void EventsStreamReactor::notifyChanged(const PowerState& powerState)
{
  Event event;
  event.mutable_powerstate()->CopyFrom(powerState);
  pushEvent(std::move(event));
}

void EventsStreamReactor::notifyChanged(const BrewProgress& brewProgress)
{
  Event event;
  event.mutable_brewprogress()->CopyFrom(brewProgress);
  pushEvent(std::move(event));
}

void EventsStreamReactor::notifyChanged(const SteamModeState& steamModeState)
{
  Event event;
  event.mutable_steammodestate()->CopyFrom(steamModeState);
  pushEvent(std::move(event));
}

void EventsStreamReactor::notifyChanged(const BrewProfile& brewProfile)
{
  Event event;
  event.mutable_brewprofile()->CopyFrom(brewProfile);
  pushEvent(std::move(event));
}

void EventsStreamReactor::pushEvent(Event&& event)
{
  {
    std::scoped_lock lock(m_queueLock);
    m_eventsQueue.push_back(std::move(event));
  }

  m_queueSize.fetch_add(1, std::memory_order_release);
  m_queueSize.notify_one();
}

void EventsStreamReactor::worker()
{
  std::list<Event> queue;
  while (true) {
    if (queue.empty()) {
      m_queueSize.wait(0, std::memory_order_acquire);
      {
        std::scoped_lock lock(m_queueLock);
        queue.splice(queue.begin(), m_eventsQueue);
      }
      m_queueSize.fetch_sub(queue.size(), std::memory_order_relaxed);
      if (queue.empty()) {
        break;
      }
    }

    m_writeStatus.store(WriteStatus::InProgress, std::memory_order_relaxed);
    StartWrite(&queue.front());

    m_writeStatus.wait(WriteStatus::InProgress, std::memory_order_acquire);
    if (m_writeStatus.load(std::memory_order_relaxed) == WriteStatus::Failed) {
      break;
    }
    queue.pop_front();
  }

  Finish(grpc::Status::OK);
}
