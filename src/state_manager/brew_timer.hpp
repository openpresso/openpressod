#ifndef BREW_TIMER_HPP
#define BREW_TIMER_HPP

#include <atomic>
#include <chrono>
#include <cstdint>

namespace openpressod
{

class BrewTimer {
  static constexpr uint32_t RUNNING_FLAG = 1 << 31;
  static constexpr uint32_t VALUE_MASK = ~(1 << 31);

public:
  void start() noexcept;
  void stop() noexcept;
  void reset() noexcept;

  std::chrono::steady_clock::duration elapsedTime() const noexcept;

private:
  static uint32_t now() noexcept;

private:
  std::atomic<uint32_t> m_state{0};
};

} // namespace openpressod

#endif // BREW_TIMER_HPP