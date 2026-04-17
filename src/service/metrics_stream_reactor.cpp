#include "metrics_stream_reactor.hpp"

#include "config/libopenpresso_config_labels.hpp"

#include <atomic>
#include <future>

#include <spdlog/spdlog.h>

using namespace openpressod;

MetricsStreamReactor::MetricsStreamReactor(const libopenpresso::CorePtr& core,
                                           const std::chrono::steady_clock::duration& updateRate)
: m_pressureSensor{core->getPressureSensor(libopenpresso_config_labels::PRESSURE_SENSOR_LABEL)}
, m_temperatureSensor{core->getTemperatureSensor(libopenpresso_config_labels::TEMPERATURE_SENSOR_LABEL)}
, m_weightSensor{core->getWeightSensor(libopenpresso_config_labels::WEIGHT_SENSOR_LABEL)}
, m_worker{&MetricsStreamReactor::worker, this, m_cancel.get_future(), updateRate}
{
}

void MetricsStreamReactor::OnWriteDone(bool ok)
{
  m_writeStatus.store(ok ? WriteStatus::Idle : WriteStatus::Failed, std::memory_order_release);
  m_writeStatus.notify_one();
}

void MetricsStreamReactor::OnCancel()
{
  try {
    m_cancel.set_value();
  }
  catch (const std::future_errc& err) {
    if (err != std::future_errc::promise_already_satisfied) {
      throw;
    }
  }
}

void MetricsStreamReactor::OnDone()
{
  m_worker.join();
  delete this;
}

void MetricsStreamReactor::worker(std::future<void> stop, std::chrono::steady_clock::duration timeout)
{
  Metrics metrics;
  while (true) {
    metrics.set_pressure(m_pressureSensor->getPressure());
    metrics.set_temperature(m_temperatureSensor->getTemperature());
    metrics.set_weight(m_weightSensor->getWeight());
    metrics.set_flowrate(m_weightSensor->getFlowRate());

    m_writeStatus.store(WriteStatus::InProgress, std::memory_order_relaxed);
    StartWrite(&metrics);

    m_writeStatus.wait(WriteStatus::InProgress, std::memory_order_acquire);
    if (m_writeStatus.load(std::memory_order_relaxed) == WriteStatus::Failed) {
      break;
    }

    if (stop.wait_for(timeout) != std::future_status::timeout) {
      break;
    }
  }

  Finish(grpc::Status::OK);
}
