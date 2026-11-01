#ifndef BREW_LOG_SAMPLER_HPP
#define BREW_LOG_SAMPLER_HPP

#include <atomic>
#include <chrono>
#include <cstddef>
#include <future>
#include <utility>

#include <libopenpresso/interfaces/libopenpresso_core.hpp>
#include <libopenpresso/interfaces/pressure_sensor.hpp>
#include <libopenpresso/interfaces/temperature_sensor.hpp>
#include <libopenpresso/interfaces/weight_sensor.hpp>

namespace openpressod
{

class OpenpressodConfig;

class BrewLogSampler {
public:
  BrewLogSampler(const libopenpresso::CorePtr& core, std::chrono::steady_clock::duration samplingInterval);

  template <typename T>
  void startSampling(T&& callback)
  {
    setCurrentStep(0);
    auto task = [stop = m_samplingStopFlag.get_future(), callback = std::forward<T>(callback), this] {
      auto nextSample = std::chrono::steady_clock::now();
      // NOLINTBEGIN(cppcoreguidelines-avoid-do-while)
      do {
        std::invoke(callback,
                    m_temperatureSensor->getTemperature(),
                    m_pressureSensor->getPressure(),
                    m_weightSensor->getWeight(),
                    m_weightSensor->getFlowRate(),
                    m_currentStep.load(std::memory_order_relaxed));
        nextSample += m_samplingInterval;
      } while (stop.wait_until(nextSample) == std::future_status::timeout);
      // NOLINTEND(cppcoreguidelines-avoid-do-while)
    };

    m_sampligTask = std::async(std::launch::async, std::move(task));
    m_isRunning = true;
  }
  void stopSampling();
  void setCurrentStep(size_t step);

private:
  bool m_isRunning = false;
  libopenpresso::PressureSensorPtr m_pressureSensor;
  libopenpresso::TemperatureSensorPtr m_temperatureSensor;
  libopenpresso::WeightSensorPtr m_weightSensor;
  std::chrono::steady_clock::duration m_samplingInterval;
  std::atomic<size_t> m_currentStep = 0;
  std::future<void> m_sampligTask;
  std::promise<void> m_samplingStopFlag;
};

} // namespace openpressod

#endif // BREW_LOG_SAMPLER_HPP