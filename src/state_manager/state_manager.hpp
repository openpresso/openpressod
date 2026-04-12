#ifndef STATE_MANAGER_HPP
#define STATE_MANAGER_HPP

#include "leds/leds_handler_interface.hpp"

#include <memory>

#include <libopenpresso/brew_steps_data.hpp>
#include <libopenpresso/types.hpp>

namespace libopenpresso::interfaces
{
class LibopenpressoCore;
class BrewProfiler;
class ControllerBase;
class TemperatureController;
class WeightSensor;
} // namespace libopenpresso::interfaces

namespace openpresso
{
class BrewProfile;
class BrewStep;
} // namespace openpresso

namespace openpressod
{

class OpenpressodConfig;

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

  bool getSteamModeState() const noexcept;
  void setSteamModeState(bool state);

  void resetScales();
  void setBrewProfile(const openpresso::BrewProfile* profile);

private:
  static libopenpresso::step_target_t getStepTarget(const openpresso::BrewStep& step);
  static libopenpresso::next_step_condition_t getStepCondition(const openpresso::BrewStep& step);
  void setAutoStopCondition(const openpresso::BrewProfile* profile);

private:
  bool m_power = false;
  bool m_steam = false;
  libopenpresso::millidegrees_t m_brewTemperature;
  brew_profiler_ptr_t m_brewProfiler;
  steam_controller_ptr_t m_steamController;
  temperature_controller_ptr_t m_temperatureController;
  weight_sensor_ptr_t m_weightSensor;
  std::unique_ptr<LedsHandlerInterface> m_leds;
};

} // namespace openpressod

#endif // STATE_MANAGER_HPP