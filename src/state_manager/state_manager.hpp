#ifndef STATE_MANAGER_HPP
#define STATE_MANAGER_HPP

#include "leds/leds_handler_interface.hpp"
#include "service/events_stream_reactor.hpp"

#include <cstddef>
#include <list>
#include <memory>

#include <libopenpresso/brew_steps_data.hpp>
#include <libopenpresso/interfaces/brew_profiler.hpp>
#include <libopenpresso/types.hpp>

namespace libopenpresso::interfaces
{
class LibopenpressoCore;
class BrewProfiler;
class ControllerBase;
class TemperatureController;
class WeightSensor;
} // namespace libopenpresso::interfaces

namespace openpressod
{

class OpenpressodConfig;
class BrewProfile;
class BrewStep;

class StateManager {
  using core_ptr_t = std::shared_ptr<libopenpresso::interfaces::LibopenpressoCore>;
  using brew_profiler_ptr_t = std::shared_ptr<libopenpresso::interfaces::BrewProfiler>;
  using steam_controller_ptr_t = std::shared_ptr<libopenpresso::interfaces::ControllerBase>;
  using temperature_controller_ptr_t = std::shared_ptr<libopenpresso::interfaces::TemperatureController>;
  using weight_sensor_ptr_t = std::shared_ptr<libopenpresso::interfaces::WeightSensor>;

public:
  StateManager(const core_ptr_t& core,
               const OpenpressodConfig& config,
               std::unique_ptr<LedsHandlerInterface>&& leds);

  bool getPowerState() const noexcept;
  void setPowerState(bool state);

  bool getBrewState() const noexcept;
  void startBrew();
  void stopBrew();
  void onBrewStepChange(size_t index);

  bool getSteamModeState() const noexcept;
  void setSteamModeState(bool state);

  void resetScales();
  void setBrewProfile(const BrewProfile* profile);
  const std::string& brewProfileName() const noexcept;
  uint64_t brewProfileHash() const noexcept;

  void addEventsStreamReactor(std::unique_ptr<EventsStreamReactor> reactor);
  void releaseEventsStreamReactor(const EventsStreamReactor* reactor);

  template <typename T>
  libopenpresso::callback_descriptor_t registerBrewProfilerCallback(T&& callback)
  {
    return m_brewProfiler->registerStepChangeCallback(std::forward<T>(callback));
  }

  void unregisterBrewProfileCallback(libopenpresso::callback_descriptor_t descr);

private:
  static libopenpresso::step_target_t getStepTarget(const BrewStep& step);
  static libopenpresso::next_step_condition_t getStepCondition(const BrewStep& step);
  void setAutoStopCondition(const BrewProfile* profile);
  static uint64_t makeProfileHash(const BrewProfile* profile);

private:
  bool m_power = false;
  bool m_steam = false;
  libopenpresso::millidegrees_t m_brewTemperature;
  brew_profiler_ptr_t m_brewProfiler;
  steam_controller_ptr_t m_steamController;
  temperature_controller_ptr_t m_temperatureController;
  weight_sensor_ptr_t m_weightSensor;
  std::unique_ptr<LedsHandlerInterface> m_leds;
  std::string m_brewProfileName;
  uint64_t m_brewProfileHash;
  std::list<std::unique_ptr<EventsStreamReactor>> m_eventsSinks;
};

} // namespace openpressod

#endif // STATE_MANAGER_HPP