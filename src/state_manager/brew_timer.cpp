#include "brew_timer.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>

using namespace openpressod;

void BrewTimer::start() noexcept
{
  m_state.store(RUNNING_FLAG | now(), std::memory_order_relaxed);
}

void BrewTimer::stop() noexcept
{
  auto state = m_state.load(std::memory_order_relaxed);
  if ((state & RUNNING_FLAG) == 0U) {
    return;
  }

  m_state.store(now() - (state & VALUE_MASK), std::memory_order_relaxed);
}

uint32_t BrewTimer::now() noexcept
{
  auto currentTime = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(currentTime.time_since_epoch()).count() &
         VALUE_MASK;
}

std::chrono::steady_clock::duration BrewTimer::elapsedTime() const noexcept
{
  auto state = m_state.load(std::memory_order_relaxed);
  if ((state & RUNNING_FLAG) == 0U) {
    return std::chrono::milliseconds{state};
  }

  return std::chrono::milliseconds{now() - (state & VALUE_MASK)};
}

void openpressod::BrewTimer::reset() noexcept
{
  m_state.store(0, std::memory_order_relaxed);
}
