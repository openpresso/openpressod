#ifndef BUTTONS_CALLBACK_REGISTRATOR_HPP
#define BUTTONS_CALLBACK_REGISTRATOR_HPP

#include "buttons_event_sink.hpp"
#include "config/openpressod_config.hpp"

#include <memory>

#include <libopenpresso/interfaces/libopenpresso_core.hpp>
#include <libopenpresso/interfaces/logical_input.hpp>
#include <libopenpresso/types.hpp>

namespace openpressod
{

class ButtonsCallbackRegistrator {
public:
  ButtonsCallbackRegistrator(const std::shared_ptr<ButtonsEventSink>& sink,
                             const libopenpresso::CorePtr& core,
                             const OpenpressodConfig& config);

  ButtonsCallbackRegistrator(const ButtonsCallbackRegistrator&) = delete;
  ButtonsCallbackRegistrator(ButtonsCallbackRegistrator&&) = delete;
  auto operator=(const ButtonsCallbackRegistrator&) = delete;
  auto operator=(ButtonsCallbackRegistrator&&) = delete;
  ~ButtonsCallbackRegistrator();

private:
  libopenpresso::callback_descriptor_t registerPowerButtonCallback(bool bistable);
  libopenpresso::callback_descriptor_t registerBrewButtonCallback(bool bistable);
  libopenpresso::callback_descriptor_t registerSteamButtonCallback(bool bistable);

private:
  std::shared_ptr<ButtonsEventSink> m_sink;
  libopenpresso::LogicalInputPtr m_powerButton;
  libopenpresso::LogicalInputPtr m_brewButton;
  libopenpresso::LogicalInputPtr m_steamButton;
  libopenpresso::callback_descriptor_t m_powerButtonCallback;
  libopenpresso::callback_descriptor_t m_brewButtonCallback;
  libopenpresso::callback_descriptor_t m_steamButtonCallback;
};

} // namespace openpressod

#endif // BUTTONS_CALLBACK_REGISTRATOR_HPP