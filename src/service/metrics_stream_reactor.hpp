#ifndef METRICS_STREAM_REACTOR_HPP
#define METRICS_STREAM_REACTOR_HPP

#include <chrono>
#include <future>
#include <thread>

#include <libopenpresso/interfaces/libopenpresso_core.hpp>
#include <libopenpresso/interfaces/pressure_sensor.hpp>
#include <libopenpresso/interfaces/temperature_sensor.hpp>
#include <libopenpresso/interfaces/weight_sensor.hpp>
#include <openpresso_proto/openpresso.grpc.pb.h>

namespace openpressod
{

class OpenpressodServiceImpl;

class MetricsStreamReactor final : public grpc::ServerWriteReactor<Metrics> {
  enum class WriteStatus : uint8_t {
    Idle,
    InProgress,
    Failed,
  };

public:
  MetricsStreamReactor(const libopenpresso::CorePtr& core,
                       const std::chrono::steady_clock::duration& updateRate);

  void OnWriteDone(bool ok) override;
  void OnCancel() override;
  void OnDone() override;

private:
  void worker(std::future<void> stop, std::chrono::steady_clock::duration timeout);

private:
  libopenpresso::PressureSensorPtr m_pressureSensor;
  libopenpresso::TemperatureSensorPtr m_temperatureSensor;
  libopenpresso::WeightSensorPtr m_weightSensor;
  std::promise<void> m_cancel;
  std::atomic<WriteStatus> m_writeStatus = WriteStatus::Idle;
  std::thread m_worker;
};

} // namespace openpressod

#endif // METRICS_STREAM_REACTOR_HPP