#ifndef METRICS_STREAM_REACTOR_HPP
#define METRICS_STREAM_REACTOR_HPP

#include <atomic>
#include <chrono>
#include <cstdint>
#include <future>
#include <memory>
#include <optional>
#include <thread>

#include <grpcpp/support/server_callback.h>
#include <libopenpresso/interfaces/libopenpresso_core.hpp>
#include <libopenpresso/interfaces/pid_controller_state.hpp>
#include <libopenpresso/interfaces/pressure_sensor.hpp>
#include <libopenpresso/interfaces/temperature_sensor.hpp>
#include <libopenpresso/interfaces/weight_sensor.hpp>
#include <libopenpresso/types.hpp>
#include <openpresso_proto/openpresso.pb.h>

namespace openpressod
{

class OpenpressodServiceImpl;

class MetricsStreamReactor final : public grpc::ServerWriteReactor<Metrics> {
  enum class WriteStatus : uint8_t {
    Idle,
    InProgress,
    Failed,
  };

  using PidStatePtr = std::shared_ptr<libopenpresso::interfaces::PidControllerState>;

public:
  MetricsStreamReactor(const libopenpresso::CorePtr& core,
                       const std::chrono::steady_clock::duration& updateRate,
                       PidSource pidSource);

  void OnWriteDone(bool ok) override;
  void OnCancel() override;
  void OnDone() override;

private:
  void worker(std::future<void> cancel, std::chrono::steady_clock::duration timeout);
  static PidStatePtr getPidControllerState(const libopenpresso::CorePtr& core,
                                           const std::optional<libopenpresso::component_label_t>& label);
  static std::optional<libopenpresso::component_label_t> pidSourceToControllerLabel(PidSource pidSource);

private:
  libopenpresso::PressureSensorPtr m_pressureSensor;
  libopenpresso::TemperatureSensorPtr m_temperatureSensor;
  libopenpresso::WeightSensorPtr m_weightSensor;
  PidStatePtr m_pidControllerState;
  std::promise<void> m_cancel;
  std::atomic<WriteStatus> m_writeStatus = WriteStatus::Idle;
  std::thread m_worker;
};

} // namespace openpressod

#endif // METRICS_STREAM_REACTOR_HPP