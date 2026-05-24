#include "config/openpressod_config.hpp"
#include "full_config.hpp"
#include "minimal_config.hpp"

#include <cstdio>
#include <cstdlib>
#include <pwd.h>
#include <unistd.h>

#include <gtest/gtest.h>

using openpressod::OpenpressodConfig;
using namespace std::chrono_literals;

static const auto EXPECTED_DAEMON_DIR = std::filesystem::path{"/var/lib/openpressod"};

TEST(OpenpressodConfig, DaemonBaseDirectory)
{
  EXPECT_EQ(EXPECTED_DAEMON_DIR, OpenpressodConfig::daemonBaseDirectory());
}

TEST(OpenpressodConfig, MissingConfigFileThrows)
{
  EXPECT_THROW(OpenpressodConfig::fromFile("/nonexistent/path/config.toml"), std::exception);
}

TEST(OpenpressodConfig, DefaultValues)
{
  auto config = OpenpressodConfig::fromString("");

  // [global] defaults
  EXPECT_EQ(EXPECTED_DAEMON_DIR / "brew_profile.json", config.brewProfilePath());
  EXPECT_EQ(EXPECTED_DAEMON_DIR / "user_settings.json", config.userSettingsPath());
  EXPECT_EQ("/dev/i2c-0", config.i2cBus());
  EXPECT_EQ("/dev/gpiochip0", config.gpioChip());
  EXPECT_FALSE(config.pidStateMonitoringEnabled());

  // [service] defaults
  EXPECT_EQ("/run/openpresso/openpresso.sock", config.socketPath());
  EXPECT_FALSE(config.tcpEnable());
  EXPECT_EQ("0.0.0.0:5001", config.tcpBindAddress());

  // [log] defaults
  EXPECT_EQ("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v", config.logMessagePattern());
  EXPECT_EQ(spdlog::level::info, config.logLevel());

  // [watchdog] defaults
  EXPECT_TRUE(config.watchdogEnabled());
  EXPECT_EQ("/dev/watchdog0", config.watchdogDevice());
  EXPECT_EQ(1s, config.watchdogTimeout());
  EXPECT_EQ(170'000, config.watchdogTemperatureLimit());

  // [buttons] defaults
  EXPECT_TRUE(config.buttonsLogicInverted());
  EXPECT_FALSE(config.powerButtonBistable());
  EXPECT_TRUE(config.brewButtonBistable());
  EXPECT_TRUE(config.steamButtonBistable());
  EXPECT_EQ(libopenpresso::PinPull::PullUp, config.buttonsPullMode());
  EXPECT_EQ(10ms, config.buttonsDebouncePeriod());

  // [leds] defaults
  EXPECT_FALSE(config.ledsLogicInverted());
  EXPECT_EQ(500, config.brewTemperatureWindow());
  EXPECT_EQ(1'000, config.steamTemperatureWindow());

  // [pressure_sensor] defaults
  EXPECT_EQ(0x48, config.pressureSensorI2cAddr());

  // [brew] default
  EXPECT_EQ(95'000, config.brewTemperature());
  EXPECT_EQ(9'000, config.brewPressure());

  // [weight_sensor] defaults
  EXPECT_EQ(600ms, config.weightSensorFilterTiming());
  EXPECT_EQ(0x2a, config.weightSensorI2cAddr());

  // [temperature_sensor] defaults
  EXPECT_EQ(750ms, config.temperatureSensorFilterTiming());
  EXPECT_EQ("/dev/spidev0.0", config.temperatureSensorSpiAddr());

  // [steam] defaults
  EXPECT_EQ(2500U, config.steamPressureThreshold());
  EXPECT_EQ(155'000, config.steamTemperature());
  EXPECT_EQ(500, config.steamRefillFlow());
}

TEST(OpenpressodConfig, MissingFieldsThrows)
{
  auto config = OpenpressodConfig::fromString("");

  // [global]
  EXPECT_THROW(config.mainsFrequency(), std::exception);
  EXPECT_THROW(config.mainsZeroCrossPin(), std::exception);

  // [buttons]
  EXPECT_THROW(config.powerButtonSignalPin(), std::exception);
  EXPECT_THROW(config.brewButtonSignalPin(), std::exception);
  EXPECT_THROW(config.steamButtonSignalPin(), std::exception);

  // [leds]
  EXPECT_THROW(config.powerLedOutputPin(), std::exception);
  EXPECT_THROW(config.brewLedOutputPin(), std::exception);
  EXPECT_THROW(config.steamLedOutputPin(), std::exception);

  // [pressure_sensor]
  EXPECT_THROW(config.pressureSensorSignalPin(), std::exception);

  // [weight_sensor]
  EXPECT_THROW(config.weightSensorScale(), std::exception);
  EXPECT_THROW(config.weightSensorSignalPin(), std::exception);

  // [temperature_sensor]
  EXPECT_THROW(config.temperatureSensorSignalPin(), std::exception);

  // [pump]
  EXPECT_THROW(config.flowControllerMultiplier(), std::exception);
  EXPECT_THROW(config.pumpOutputPin(), std::exception);
  EXPECT_THROW(config.pumpVolumePerPulse(), std::exception);
  EXPECT_THROW(config.pumpStallPressure(), std::exception);

  // [valve]
  EXPECT_THROW(config.valveOutputPin(), std::exception);

  // [heater]
  EXPECT_THROW(config.heaterOutputPin(), std::exception);

  // [brew.pid_setting]
  EXPECT_THROW(config.brewTemperatureControllerPidSettings(), std::exception);

  // [steam] pid settings
  EXPECT_THROW(config.steamPreheatTemperatureControllerPidSettings(), std::exception);
  EXPECT_THROW(config.steamActiveTemperatureControllerPidSettings(), std::exception);
}

TEST(OpenpressodConfig, MinimalConfigRequiredFields)
{
  auto config = OpenpressodConfig::fromString(MINIMAL_CONFIG);

  // [global]
  EXPECT_EQ(50, config.mainsFrequency());
  EXPECT_EQ(17, config.mainsZeroCrossPin());

  // [buttons]
  EXPECT_EQ(26, config.powerButtonSignalPin());
  EXPECT_EQ(19, config.brewButtonSignalPin());
  EXPECT_EQ(13, config.steamButtonSignalPin());

  // [leds]
  EXPECT_EQ(21, config.powerLedOutputPin());
  EXPECT_EQ(16, config.brewLedOutputPin());
  EXPECT_EQ(20, config.steamLedOutputPin());

  // [pressure_sensor]
  EXPECT_EQ(4, config.pressureSensorSignalPin());

  // [weight_sensor]
  EXPECT_EQ(27762U, config.weightSensorScale());
  EXPECT_EQ(12, config.weightSensorSignalPin());

  // [temperature_sensor]
  EXPECT_EQ(25, config.temperatureSensorSignalPin());

  // [pump]
  EXPECT_FLOAT_EQ(0.00005F, config.flowControllerMultiplier());
  EXPECT_EQ(27, config.pumpOutputPin());
  EXPECT_EQ(205000U, config.pumpVolumePerPulse());
  EXPECT_EQ(18500U, config.pumpStallPressure());

  // [heater]
  EXPECT_EQ(23, config.heaterOutputPin());

  // [valve]
  EXPECT_EQ(22, config.valveOutputPin());

  // [brew.pid_settings]
  auto brewPid = config.brewTemperatureControllerPidSettings();
  EXPECT_FLOAT_EQ(0.07f, brewPid.p);
  EXPECT_FLOAT_EQ(0.25f, brewPid.d);
  EXPECT_FLOAT_EQ(0.025f, brewPid.dTermRelax);
  EXPECT_EQ(750ms, brewPid.dFilterTime);
  EXPECT_FLOAT_EQ(0.0025f, brewPid.i);
  EXPECT_FLOAT_EQ(1.0f, brewPid.iTermRelax);
  EXPECT_FLOAT_EQ(0.04f, brewPid.f);
  EXPECT_FLOAT_EQ(0.002f, brewPid.w);
  EXPECT_FLOAT_EQ(0.02f, brewPid.wDecay);

  // [steam.preheat_pid_settings]
  auto steamPreheatPid = config.steamPreheatTemperatureControllerPidSettings();
  EXPECT_FLOAT_EQ(0.08f, steamPreheatPid.p);
  EXPECT_FLOAT_EQ(0.2f, steamPreheatPid.d);
  EXPECT_FLOAT_EQ(0.05f, steamPreheatPid.dTermRelax);
  EXPECT_EQ(750ms, steamPreheatPid.dFilterTime);
  EXPECT_FLOAT_EQ(0.01f, steamPreheatPid.i);
  EXPECT_FLOAT_EQ(1.5f, steamPreheatPid.iTermRelax);
  EXPECT_FLOAT_EQ(0.0f, steamPreheatPid.f);
  EXPECT_FLOAT_EQ(0.0f, steamPreheatPid.w);
  EXPECT_FLOAT_EQ(0.0f, steamPreheatPid.wDecay);

  // [steam.active_pid_settings]
  auto steamActivePid = config.steamActiveTemperatureControllerPidSettings();
  EXPECT_FLOAT_EQ(0.25f, steamActivePid.p);
  EXPECT_FLOAT_EQ(0.5f, steamActivePid.d);
  EXPECT_FLOAT_EQ(0.0f, steamActivePid.dTermRelax);
  EXPECT_EQ(750ms, steamActivePid.dFilterTime);
  EXPECT_FLOAT_EQ(0.02f, steamActivePid.i);
  EXPECT_FLOAT_EQ(0.5f, steamActivePid.iTermRelax);
  EXPECT_FLOAT_EQ(0.0f, steamActivePid.f);
  EXPECT_FLOAT_EQ(0.0f, steamActivePid.w);
  EXPECT_FLOAT_EQ(0.0f, steamActivePid.wDecay);
}

TEST(OpenpressodConfig, FullConfig)
{
  auto config = OpenpressodConfig::fromString(FULL_CONFIG);

  // [global]
  EXPECT_EQ(60u, config.mainsFrequency());
  EXPECT_EQ(5, config.mainsZeroCrossPin());
  EXPECT_EQ("/custom/profile.json", config.brewProfilePath());
  EXPECT_EQ("/custom/user_settings.json", config.userSettingsPath());
  EXPECT_EQ("/dev/i2c-1", config.i2cBus());
  EXPECT_EQ("/dev/gpiochip4", config.gpioChip());
  EXPECT_TRUE(config.pidStateMonitoringEnabled());

  // [service]
  EXPECT_EQ("/tmp/custom.sock", config.socketPath());
  EXPECT_TRUE(config.tcpEnable());
  EXPECT_EQ("127.0.0.1:80", config.tcpBindAddress());

  // [log]
  EXPECT_EQ("[%l] %v", config.logMessagePattern());
  EXPECT_EQ(spdlog::level::warn, config.logLevel());

  // [watchdog]
  EXPECT_FALSE(config.watchdogEnabled());
  EXPECT_EQ(5s, config.watchdogTimeout());
  EXPECT_EQ("/dev/watchdog1", config.watchdogDevice());
  EXPECT_EQ(180'000, config.watchdogTemperatureLimit());

  // [buttons]
  EXPECT_FALSE(config.buttonsLogicInverted());
  EXPECT_TRUE(config.powerButtonBistable());
  EXPECT_FALSE(config.brewButtonBistable());
  EXPECT_FALSE(config.steamButtonBistable());
  EXPECT_EQ(libopenpresso::PinPull::PullDown, config.buttonsPullMode());
  EXPECT_EQ(1, config.powerButtonSignalPin());
  EXPECT_EQ(2, config.brewButtonSignalPin());
  EXPECT_EQ(3, config.steamButtonSignalPin());
  EXPECT_EQ(2ms, config.buttonsDebouncePeriod());

  // [leds]
  EXPECT_TRUE(config.ledsLogicInverted());
  EXPECT_EQ(10, config.powerLedOutputPin());
  EXPECT_EQ(11, config.brewLedOutputPin());
  EXPECT_EQ(12, config.steamLedOutputPin());

  // [pressure_sensor]
  EXPECT_EQ(7, config.pressureSensorSignalPin());
  EXPECT_EQ(73, config.pressureSensorI2cAddr());

  // [weight_sensor]
  EXPECT_EQ(50000u, config.weightSensorScale());
  EXPECT_EQ(500ms, config.weightSensorFilterTiming());
  EXPECT_EQ(8, config.weightSensorSignalPin());
  EXPECT_EQ(30, config.weightSensorI2cAddr());

  // [temperature_sensor]
  EXPECT_EQ("/dev/spidev1.0", config.temperatureSensorSpiAddr());
  EXPECT_EQ(150ms, config.temperatureSensorFilterTiming());
  EXPECT_EQ(9, config.temperatureSensorSignalPin());

  // [pump]
  EXPECT_FLOAT_EQ(0.0001f, config.flowControllerMultiplier());
  EXPECT_EQ(14, config.pumpOutputPin());
  EXPECT_EQ(300000u, config.pumpVolumePerPulse());
  EXPECT_EQ(20000u, config.pumpStallPressure());

  // [heater]
  EXPECT_EQ(15, config.heaterOutputPin());

  // [valve]
  EXPECT_EQ(6, config.valveOutputPin());

  // [steam]
  EXPECT_EQ(3000u, config.steamPressureThreshold());
  EXPECT_EQ(160'000, config.steamTemperature());
  EXPECT_EQ(600, config.steamRefillFlow());
  EXPECT_EQ(1'300, config.steamTemperatureWindow());

  // [brew]
  EXPECT_EQ(92'000, config.brewTemperature());
  EXPECT_EQ(8'500, config.brewPressure());
  EXPECT_EQ(200, config.brewTemperatureWindow());

  // [brew.pid_settings]
  auto brewPid = config.brewTemperatureControllerPidSettings();
  EXPECT_FLOAT_EQ(0.1f, brewPid.p);
  EXPECT_FLOAT_EQ(0.3f, brewPid.d);
  EXPECT_FLOAT_EQ(0.03f, brewPid.dTermRelax);
  EXPECT_EQ(150ms, brewPid.dFilterTime);
  EXPECT_FLOAT_EQ(0.005f, brewPid.i);
  EXPECT_FLOAT_EQ(1.5f, brewPid.iTermRelax);
  EXPECT_FLOAT_EQ(0.06f, brewPid.f);
  EXPECT_FLOAT_EQ(0.003f, brewPid.w);
  EXPECT_FLOAT_EQ(0.03f, brewPid.wDecay);

  // [steam.preheat_pid_settings]
  auto steamPreheatPid = config.steamPreheatTemperatureControllerPidSettings();
  EXPECT_FLOAT_EQ(0.09f, steamPreheatPid.p);
  EXPECT_FLOAT_EQ(0.3f, steamPreheatPid.d);
  EXPECT_FLOAT_EQ(0.06f, steamPreheatPid.dTermRelax);
  EXPECT_EQ(150ms, steamPreheatPid.dFilterTime);
  EXPECT_FLOAT_EQ(0.02f, steamPreheatPid.i);
  EXPECT_FLOAT_EQ(2.0f, steamPreheatPid.iTermRelax);
  EXPECT_FLOAT_EQ(0.0f, steamPreheatPid.f);
  EXPECT_FLOAT_EQ(0.0f, steamPreheatPid.w);
  EXPECT_FLOAT_EQ(0.0f, steamPreheatPid.wDecay);

  // [steam.active_pid_settings]
  auto steamActivePid = config.steamActiveTemperatureControllerPidSettings();
  EXPECT_FLOAT_EQ(0.3f, steamActivePid.p);
  EXPECT_FLOAT_EQ(0.6f, steamActivePid.d);
  EXPECT_FLOAT_EQ(0.01f, steamActivePid.dTermRelax);
  EXPECT_EQ(150ms, steamActivePid.dFilterTime);
  EXPECT_FLOAT_EQ(0.03f, steamActivePid.i);
  EXPECT_FLOAT_EQ(0.6f, steamActivePid.iTermRelax);
  EXPECT_FLOAT_EQ(0.0f, steamActivePid.f);
  EXPECT_FLOAT_EQ(0.0f, steamActivePid.w);
  EXPECT_FLOAT_EQ(0.0f, steamActivePid.wDecay);
}
