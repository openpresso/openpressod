#ifndef HEATING_STATUS_MANAGER_HPP
#define HEATING_STATUS_MANAGER_HPP

#include "leds/leds_handler.hpp"

#include <atomic>
#include <functional>
#include <memory>
#include <optional>

#include <libopenpresso/interfaces/libopenpresso_core.hpp>
#include <libopenpresso/interfaces/temperature_sensor.hpp>
#include <libopenpresso/types.hpp>
#include <openpresso_proto/openpresso.pb.h>

namespace openpressod
{

class OpenpressodConfig;

class StatusManager {
public:
  StatusManager(const libopenpresso::CorePtr& core,
                const OpenpressodConfig& config,
                std::unique_ptr<LedsHandler>&& leds);

  StatusManager(const StatusManager&) = delete;
  StatusManager(StatusManager&&) = delete;
  auto operator=(const StatusManager&) = delete;
  auto operator=(StatusManager&&) = delete;

  ~StatusManager();

  HeatingStatus getCurrentStatus() const noexcept;

  void setCallback(std::function<void(HeatingStatus)> callback);

  void disable();
  void indicateBrewState(libopenpresso::millidegrees_t targetTemp);
  void indicateSteamState(libopenpresso::millidegrees_t targetTemp);

private:
  std::function<void(HeatingStatus)> m_callback = [](auto) {};

  std::atomic<HeatingStatus> m_heatingStatus = HeatingStatus::HeatingStatus_DISABLED;
  HeatingStatus m_heatingStatusCached = HeatingStatus::HeatingStatus_DISABLED;

  libopenpresso::millidegrees_t m_brewTempretureWindow;
  libopenpresso::millidegrees_t m_steamTempretureWindow;
  std::unique_ptr<LedsHandler> m_leds;
  libopenpresso::TemperatureSensorPtr m_temperatureSensor;
  std::optional<libopenpresso::callback_descriptor_t> m_temperatureCallback;
};

} // namespace openpressod

#endif // HEATING_STATUS_MANAGER_HPP