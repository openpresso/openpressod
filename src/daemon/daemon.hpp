#ifndef DAEMON_HPP
#define DAEMON_HPP

#include "buttons/buttons_callback_registrator.hpp"
#include "service/service_manager.hpp"

#include <memory>
#include <string>

namespace openpressod
{

class BrewProfileManager;
class UserSettingsManager;
class OpenpressodConfig;

namespace db::brew_history
{
class AsyncBrewHistoryRepository;
}

class Daemon {
public:
  explicit Daemon(const std::string& configPath);

private:
  static void setupLogger(const OpenpressodConfig& config);
  static std::unique_ptr<BrewProfileManager> setupBrewProfileManager(const OpenpressodConfig& config);
  static std::unique_ptr<UserSettingsManager> setupUserSettingsManager(const OpenpressodConfig& config);
  static std::shared_ptr<db::brew_history::AsyncBrewHistoryRepository> setupHistoryRepository(
    const OpenpressodConfig& config);

private:
  std::unique_ptr<ButtonsCallbackRegistrator> m_buttonsHandler;
  std::unique_ptr<ServiceManager> m_service;
};

} // namespace openpressod

#endif // DAEMON_HPP
