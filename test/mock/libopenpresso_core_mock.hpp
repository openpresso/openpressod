#ifndef LIBOPENPRESSO_CORE_MOCK_HPP
#define LIBOPENPRESSO_CORE_MOCK_HPP

#include <gmock/gmock.h>
#include <libopenpresso/interfaces/libopenpresso_core.hpp>

class MockLibopenpressoCore : public libopenpresso::interfaces::LibopenpressoCore {
public:
  MOCK_METHOD(libopenpresso::WeightSensorPtr,
              getWeightSensor,
              (const libopenpresso::component_label_t& label),
              (override));
  MOCK_METHOD(libopenpresso::FlowRateControllerPtr,
              getFlowRateController,
              (const libopenpresso::component_label_t& label),
              (override));
  MOCK_METHOD(libopenpresso::PressureSensorPtr,
              getPressureSensor,
              (const libopenpresso::component_label_t& label),
              (override));
  MOCK_METHOD(libopenpresso::PressureControllerPtr,
              getPressureController,
              (const libopenpresso::component_label_t& label),
              (override));
  MOCK_METHOD(libopenpresso::TemperatureSensorPtr,
              getTemperatureSensor,
              (const libopenpresso::component_label_t& label),
              (override));
  MOCK_METHOD(libopenpresso::TemperatureControllerPtr,
              getTemperatureController,
              (const libopenpresso::component_label_t& label),
              (override));
  MOCK_METHOD(libopenpresso::LogicalOutputPtr,
              getLogicalOutput,
              (const libopenpresso::component_label_t& label),
              (override));
  MOCK_METHOD(libopenpresso::LogicalInputPtr,
              getLogicalInput,
              (const libopenpresso::component_label_t& label),
              (override));
  MOCK_METHOD(libopenpresso::BrewProfilerPtr,
              getBrewProfiler,
              (const libopenpresso::component_label_t& label),
              (override));
  MOCK_METHOD(libopenpresso::ControllerBasePtr,
              getSteamController,
              (const libopenpresso::component_label_t& label),
              (override));
};

#endif // LIBOPENPRESSO_CORE_MOCK_HPP