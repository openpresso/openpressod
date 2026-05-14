#ifndef STATE_MANAGER_HPP
#define STATE_MANAGER_HPP

#include "leds/leds_handler_interface.hpp"

#include <memory>
#include <ranges>
#include <utility>

#include <libopenpresso/interfaces/brew_profiler.hpp>
#include <libopenpresso/types.hpp>

namespace libopenpresso::interfaces
{
class LibopenpressoCore;
class BrewProfiler;
class TemperatureController;
class WeightSensor;
} // namespace libopenpresso::interfaces

namespace openpressod
{

class StateManager {
  using core_ptr_t = std::shared_ptr<libopenpresso::interfaces::LibopenpressoCore>;
  using brew_profiler_ptr_t = std::shared_ptr<libopenpresso::interfaces::BrewProfiler>;
  using temperature_controller_ptr_t = std::shared_ptr<libopenpresso::interfaces::TemperatureController>;
  using weight_sensor_ptr_t = std::shared_ptr<libopenpresso::interfaces::WeightSensor>;

public:
  StateManager(const core_ptr_t& core, std::unique_ptr<LedsHandlerInterface>&& leds);

  bool getPowerState() const noexcept;
  void setPowerState(bool state);

  bool getBrewState() const noexcept;
  void startBrew();
  void stopBrew();

  bool getSteamModeState() const noexcept;
  void setSteamModeState(bool state);

  void resetScales();

  void setBrewTemperature(libopenpresso::millidegrees_t temperature);
  void setSteamTemperature(libopenpresso::millidegrees_t temperature);

  template <std::ranges::range Steps>
  void setBrewSteps(const Steps& steps)
  {
    if (m_brewProfiler->isActive()) {
      throw std::runtime_error{"Cannot set brew steps sequence brewing"};
    }
    m_brewProfiler->setSteps({steps.begin(), steps.end()});
  }

  template <typename T>
  void setAutoStopCondition(T&& cond)
  {
    if (m_brewProfiler->isActive()) {
      throw std::runtime_error{"Cannot change auto stop condition while brewing"};
    }
    m_brewProfiler->setAutoStopCondition(std::forward<T>(cond));
  }

  template <typename T>
  libopenpresso::callback_descriptor_t registerBrewProfilerCallback(T&& callback)
  {
    return m_brewProfiler->registerStepChangeCallback(std::forward<T>(callback));
  }

  void unregisterBrewProfileCallback(libopenpresso::callback_descriptor_t descr);

private:
  bool m_power = false;
  bool m_steam = false;
  brew_profiler_ptr_t m_brewProfiler;
  temperature_controller_ptr_t m_brewTemperatureController;
  temperature_controller_ptr_t m_steamController;
  weight_sensor_ptr_t m_weightSensor;
  std::unique_ptr<LedsHandlerInterface> m_leds;
};

} // namespace openpressod

#endif // STATE_MANAGER_HPP