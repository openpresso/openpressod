#include "brew_history_stream_reactor.hpp"

#include "brew_history/brew_history_manager.hpp"
#include "database/database_id.hpp"
#include "protobuf_helpers.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <list>
#include <mutex>
#include <utility>

#include <grpcpp/support/status.h>
#include <openpresso_proto/brew_history.pb.h>
#include <openpresso_proto/brew_profile.pb.h>

using namespace openpressod;

BrewHistoryStreamReactor::BrewHistoryStreamReactor(BrewHistoryManager* owner, DatabaseId brewId)
: m_brewId{brewId}
, m_owner{owner}
, m_worker{&BrewHistoryStreamReactor::worker, this}
{
}

void BrewHistoryStreamReactor::OnWriteDone(bool ok)
{
  m_writeStatus.store(ok ? WriteStatus::Idle : WriteStatus::Failed, std::memory_order_release);
  m_writeStatus.notify_one();
}

void BrewHistoryStreamReactor::OnCancel()
{
  {
    std::scoped_lock lock(m_queueLock);
    m_cancelled = true;
  }
  m_queueCv.notify_one();
}

void BrewHistoryStreamReactor::OnDone()
{
  m_worker.join();
  m_owner->removeStreamReactor(this);
}

DatabaseId BrewHistoryStreamReactor::brewId() const noexcept
{
  return m_brewId;
}

void BrewHistoryStreamReactor::sendBrewStop()
{
  pushMessage(makeBrewStop());
}

void BrewHistoryStreamReactor::pushMessage(BrewHistoryStream message)
{
  {
    std::scoped_lock lock(m_queueLock);
    m_messagesQueue.push_back(std::move(message));
  }

  m_queueCv.notify_one();
}

void BrewHistoryStreamReactor::pushMessages(std::list<BrewHistoryStream>&& messagesQueue)
{
  {
    std::scoped_lock lock(m_queueLock);
    m_messagesQueue.splice(m_messagesQueue.end(), std::move(messagesQueue));
  }

  m_queueCv.notify_one();
}

BrewHistoryStream BrewHistoryStreamReactor::makeBrewStart(std::chrono::steady_clock::duration samplingInterval,
                                                          std::chrono::system_clock::time_point startPoint,
                                                          BrewProfile profile)
{
  using namespace protobuf_helpers;

  BrewHistoryStream message;
  toProtoDuration(samplingInterval, *message.mutable_brewhistorystart()->mutable_samplinginterval());
  toProtoTimestamp(startPoint, *message.mutable_brewhistorystart()->mutable_startpoint());
  *message.mutable_brewhistorystart()->mutable_profile() = std::move(profile);
  return message;
}

BrewHistoryStream BrewHistoryStreamReactor::makeBrewStop()
{
  BrewHistoryStream message;
  message.mutable_brewhistorystop();
  return message;
}

void BrewHistoryStreamReactor::sendEntryNotFound()
{
  {
    std::scoped_lock lock(m_queueLock);
    m_notFound = true;
  }
  m_queueCv.notify_one();
}

void BrewHistoryStreamReactor::worker()
{
  std::list<BrewHistoryStream> queue;
  while (true) {
    if (queue.empty()) {
      std::unique_lock lock(m_queueLock);
      m_queueCv.wait(lock, [this] { return m_cancelled || m_notFound || !m_messagesQueue.empty(); });
      if (m_cancelled || m_notFound) {
        break;
      }
      queue.splice(queue.begin(), m_messagesQueue);
    }

    m_writeStatus.store(WriteStatus::InProgress, std::memory_order_relaxed);
    StartWrite(&queue.front());

    m_writeStatus.wait(WriteStatus::InProgress, std::memory_order_acquire);
    if (m_writeStatus.load(std::memory_order_relaxed) == WriteStatus::Failed ||
        queue.front().has_brewhistorystop()) {
      break;
    }
    queue.pop_front();
  }

  Finish(m_notFound ? grpc::Status{grpc::StatusCode::NOT_FOUND, "Brew history entry not found"}
                    : grpc::Status::OK);
}
