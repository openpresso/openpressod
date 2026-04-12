#ifndef LEDS_HANDLER_HPP
#define LEDS_HANDLER_HPP

#include "config/openpressod_config.hpp"
#include "leds_handler_interface.hpp"

#include <optional>

#include <libopenpresso/interfaces/libopenpresso_core.hpp>
#include <libopenpresso/interfaces/logical_output.hpp>
#include <libopenpresso/interfaces/temperature_sensor.hpp>
#include <libopenpresso/types.hpp>

namespace openpressod
{

class LedsHandler : public LedsHandlerInterface {
public:
  LedsHandler(const libopenpresso::CorePtr& core, const OpenpressodConfig& config);
  LedsHandler(const LedsHandler&) = delete;
  LedsHandler(LedsHandler&&) = delete;
  auto operator=(const LedsHandler&) = delete;
  auto operator=(LedsHandler&&) = delete;
  ~LedsHandler();

  void indicatePowerOff() override;
  void indicateBrewState(libopenpresso::millidegrees_t targetTemp) override;
  void indicateSteamState() override;

private:
  libopenpresso::millidegrees_t m_brewTempretureWindow;
  libopenpresso::millidegrees_t m_steamTempreture;
  libopenpresso::millidegrees_t m_steamTempretureWindow;
  libopenpresso::LogicalOutputPtr m_power;
  libopenpresso::LogicalOutputPtr m_brew;
  libopenpresso::LogicalOutputPtr m_steam;
  libopenpresso::TemperatureSensorPtr m_temperatureSensor;
  std::optional<libopenpresso::callback_descriptor_t> m_temperatureCallback;
};

} // namespace openpressod

#endif // LEDS_HANDLER_HPP