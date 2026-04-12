#ifndef SIGNAL_HANDLER_HPP
#define SIGNAL_HANDLER_HPP

#include <atomic>

namespace openpressod
{

class SignalsHandler {
public:
  static void init();
  static const std::atomic<bool>& exitFlag() noexcept;
  static void exitSignalsHandler([[maybe_unused]] int signal);
  static void registerExitHandler(int signal);

  SignalsHandler() = delete;

private:
  static std::atomic<bool> s_exit;
};

} // namespace openpressod

#endif // SIGNAL_HANDLER_HPP