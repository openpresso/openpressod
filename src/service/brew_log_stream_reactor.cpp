#include "brew_log_stream_reactor.hpp"

#include "brew_log/brew_log_manager.hpp"
#include "protobuf_helpers.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <list>
#include <mutex>
#include <utility>

#include <grpcpp/support/status.h>
#include <openpresso_proto/brew_profile.pb.h>
#include <openpresso_proto/openpresso.pb.h>

using namespace openpressod;

BrewLogStreamReactor::BrewLogStreamReactor(BrewLogManager* owner, uint32_t brewId)
: m_brewId{brewId}
, m_owner{owner}
, m_worker{&BrewLogStreamReactor::worker, this}
{
}

void BrewLogStreamReactor::OnWriteDone(bool ok)
{
  m_writeStatus.store(ok ? WriteStatus::Idle : WriteStatus::Failed, std::memory_order_release);
  m_writeStatus.notify_one();
}

void BrewLogStreamReactor::OnCancel()
{
  {
    std::scoped_lock lock(m_queueLock);
    m_cancelled = true;
  }
  m_queueCv.notify_one();
}

void BrewLogStreamReactor::OnDone()
{
  m_worker.join();
  m_owner->removeStreamReactor(this);
}

uint32_t BrewLogStreamReactor::brewId() const noexcept
{
  return m_brewId;
}

void BrewLogStreamReactor::sendBrewStop()
{
  pushRecord(makeBrewStop());
}

void BrewLogStreamReactor::pushRecord(BrewLogStream record)
{
  {
    std::scoped_lock lock(m_queueLock);
    m_recordsQueue.push_back(std::move(record));
  }

  m_queueCv.notify_one();
}

void BrewLogStreamReactor::pushRecords(std::list<BrewLogStream>&& recordsQueue)
{
  {
    std::scoped_lock lock(m_queueLock);
    m_recordsQueue.splice(m_recordsQueue.end(), std::move(recordsQueue));
  }

  m_queueCv.notify_one();
}

BrewLogStream BrewLogStreamReactor::makeBrewStart(std::chrono::steady_clock::duration samplingInterval,
                                                  std::chrono::system_clock::time_point startPoint,
                                                  BrewProfile profile)
{
  using namespace protobuf_helpers;

  BrewLogStream brewRecord;
  toProtoDuration(samplingInterval, *brewRecord.mutable_brewlogstart()->mutable_samplinginterval());
  toProtoTimestamp(startPoint, *brewRecord.mutable_brewlogstart()->mutable_startpoint());
  *brewRecord.mutable_brewlogstart()->mutable_profile() = std::move(profile);
  return brewRecord;
}

BrewLogStream BrewLogStreamReactor::makeBrewStop()
{
  BrewLogStream brewRecord;
  brewRecord.mutable_brewlogstop();
  return brewRecord;
}

void BrewLogStreamReactor::sendLogNotFound()
{
  {
    std::scoped_lock lock(m_queueLock);
    m_notFound = true;
  }
  m_queueCv.notify_one();
}

void BrewLogStreamReactor::worker()
{
  std::list<BrewLogStream> queue;
  while (true) {
    if (queue.empty()) {
      std::unique_lock lock(m_queueLock);
      m_queueCv.wait(lock, [this] { return m_cancelled || m_notFound || !m_recordsQueue.empty(); });
      if (m_cancelled || m_notFound) {
        break;
      }
      queue.splice(queue.begin(), m_recordsQueue);
    }

    m_writeStatus.store(WriteStatus::InProgress, std::memory_order_relaxed);
    StartWrite(&queue.front());

    m_writeStatus.wait(WriteStatus::InProgress, std::memory_order_acquire);
    if (m_writeStatus.load(std::memory_order_relaxed) == WriteStatus::Failed ||
        queue.front().has_brewlogstop()) {
      break;
    }
    queue.pop_front();
  }

  Finish(m_notFound ? grpc::Status{grpc::StatusCode::NOT_FOUND, "Brew log not found"} : grpc::Status::OK);
}
