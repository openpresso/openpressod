#include "brew_log_stream_reactor.hpp"

#include "brew_log/brew_log_manager.hpp"
#include "protobuf_helpers.hpp"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <list>
#include <mutex>
#include <utility>

#include <openpresso_proto/openpresso.pb.h>

using namespace openpressod;

BrewLogStreamReactor::BrewLogStreamReactor(BrewLogManager* owner)
: m_owner{owner}
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

void BrewLogStreamReactor::sendBrewStart(std::chrono::steady_clock::duration samplingInterval,
                                         std::chrono::system_clock::time_point startPoint)
{
  using namespace protobuf_helpers;

  BrewLogRecord brewRecord;
  toProtoDuration(samplingInterval, *brewRecord.mutable_brewlogstart()->mutable_samplinginterval());
  toProtoTimestamp(startPoint, *brewRecord.mutable_brewlogstart()->mutable_startpoint());
  pushRecord(std::move(brewRecord));
  m_currentStep = 0;
}

void BrewLogStreamReactor::sendBrewStop(size_t finalStep)
{
  std::list<BrewLogRecord> queue;
  processStepChange(finalStep, queue);

  BrewLogRecord brewRecord;
  brewRecord.mutable_stopflag();
  if (queue.empty()) {
    pushRecord(std::move(brewRecord));
  }
  else {
    queue.push_back(std::move(brewRecord));
    pushRecords(std::move(queue));
  }
}

void BrewLogStreamReactor::sendLogCleanUp()
{
  BrewLogRecord brewRecord;
  brewRecord.mutable_logcleanedup();
  pushRecord(std::move(brewRecord));
}

void BrewLogStreamReactor::processStepChange(size_t stepIndex, std::list<BrewLogRecord>& queue)
{
  for (size_t step = m_currentStep + 1; step <= stepIndex; ++step) {
    BrewLogRecord brewRecord;
    brewRecord.mutable_stepchange()->set_brewstepindex(step);
    queue.push_back(std::move(brewRecord));
  }
  m_currentStep = stepIndex;
}

void BrewLogStreamReactor::pushRecord(BrewLogRecord&& record)
{
  {
    std::scoped_lock lock(m_queueLock);
    m_recordsQueue.push_back(std::move(record));
  }

  m_queueCv.notify_one();
}

void BrewLogStreamReactor::pushRecords(std::list<BrewLogRecord>&& recordsQueue)
{
  {
    std::scoped_lock lock(m_queueLock);
    m_recordsQueue.splice(m_recordsQueue.end(), std::move(recordsQueue));
  }

  m_queueCv.notify_one();
}

void BrewLogStreamReactor::worker()
{
  std::list<BrewLogRecord> queue;
  while (true) {
    if (queue.empty()) {
      std::unique_lock lock(m_queueLock);
      m_queueCv.wait(lock, [this] { return m_cancelled || !m_recordsQueue.empty(); });
      if (m_cancelled) {
        break;
      }
      queue.splice(queue.begin(), m_recordsQueue);
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
