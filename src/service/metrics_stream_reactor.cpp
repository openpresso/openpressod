#include "metrics_stream_reactor.hpp"

#include "config/libopenpresso_config_labels.hpp"

#include <atomic>
#include <chrono>
#include <format>
#include <future>
#include <memory>
#include <optional>
#include <stdexcept>

#include <libopenpresso/interfaces/libopenpresso_core.hpp>
#include <libopenpresso/interfaces/pid_controller_state.hpp>
#include <openpresso_proto/openpresso.pb.h>

using namespace openpressod;

MetricsStreamReactor::MetricsStreamReactor(const libopenpresso::CorePtr& core,
                                           const std::chrono::steady_clock::duration& updateRate,
                                           PidSource pidSource)
: m_pressureSensor{core->getPressureSensor(libopenpresso_config_labels::PRESSURE_SENSOR_LABEL)}
, m_temperatureSensor{core->getTemperatureSensor(libopenpresso_config_labels::TEMPERATURE_SENSOR_LABEL)}
, m_weightSensor{core->getWeightSensor(libopenpresso_config_labels::WEIGHT_SENSOR_LABEL)}
, m_pidControllerState{getPidControllerState(core, pidSourceToControllerLabel(pidSource))}
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

    if (m_pidControllerState) {
      metrics.mutable_pidmetrics()->set_p(m_pidControllerState->pTerm());
      metrics.mutable_pidmetrics()->set_i(m_pidControllerState->iTerm());
      metrics.mutable_pidmetrics()->set_d(m_pidControllerState->dTerm());
      metrics.mutable_pidmetrics()->set_f(m_pidControllerState->fTerm());
      metrics.mutable_pidmetrics()->set_sum(m_pidControllerState->pidSum());
    }

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

MetricsStreamReactor::PidStatePtr MetricsStreamReactor::getPidControllerState(
  const libopenpresso::CorePtr& core, const std::optional<libopenpresso::component_label_t>& label)
{
  if (!label.has_value()) {
    return {};
  }

  auto controller = std::dynamic_pointer_cast<libopenpresso::interfaces::PidControllerState>(
    core->getTemperatureController(label.value()));

  if (!controller) {
    throw std::runtime_error{
      std::format("Cannot obtain pid controller statet for label: {}", label.value())};
  }

  return controller;
}

std::optional<libopenpresso::component_label_t> MetricsStreamReactor::pidSourceToControllerLabel(PidSource pidSource)
{
  using namespace libopenpresso_config_labels;
  switch (pidSource) {
  case PidSource::BrewTemperatureController:
    return BREW_TEMPERATURE_CONTROLLER_LABEL;
  case PidSource::SteamPreheatTemperatureController:
    return STEAM_PREHEAT_TEMPERATURE_CONTROLLER_LABEL;
  case PidSource::SteamActiveTemperatureController:
    return STEAM_ACTIVE_TEMPERATURE_CONTROLLER_LABEL;
  default:
    return std::nullopt;
  }
}
