#include "brew_log_sampler.hpp"

#include "config/libopenpresso_config_labels.hpp"

using namespace openpressod;

BrewLogSampler::BrewLogSampler(const libopenpresso::CorePtr& core,
                               std::chrono::steady_clock::duration samplingInterval)
: m_pressureSensor{core->getPressureSensor(libopenpresso_config_labels::PRESSURE_SENSOR_LABEL)}
, m_temperatureSensor{core->getTemperatureSensor(libopenpresso_config_labels::TEMPERATURE_SENSOR_LABEL)}
, m_weightSensor{core->getWeightSensor(libopenpresso_config_labels::WEIGHT_SENSOR_LABEL)}
, m_samplingInterval{samplingInterval}
{
}

void BrewLogSampler::stopSampling()
{
  if (!m_isRunning) {
    return;
  }

  m_samplingStopFlag.set_value();
  m_sampligTask.get();

  m_isRunning = false;
  m_samplingStopFlag = {};
}
