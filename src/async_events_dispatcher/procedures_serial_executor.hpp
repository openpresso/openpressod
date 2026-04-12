#ifndef PROCEDURES_SERIAL_EXECUTOR_HPP
#define PROCEDURES_SERIAL_EXECUTOR_HPP

#include <atomic>
#include <cstddef>
#include <functional>
#include <future>
#include <list>
#include <mutex>
#include <thread>
#include <type_traits>
#include <utility>

namespace openpressod
{

class ProceduresSerialExecutor {
  using queue_t = std::list<std::move_only_function<void()>>;

public:
  ProceduresSerialExecutor()
  : m_worker{&ProceduresSerialExecutor::worker, this}
  {
  }
  ProceduresSerialExecutor(const ProceduresSerialExecutor&) = delete;
  ProceduresSerialExecutor(ProceduresSerialExecutor&&) = delete;
  auto operator=(const ProceduresSerialExecutor&) = delete;
  auto operator=(ProceduresSerialExecutor&&) = delete;
  ~ProceduresSerialExecutor()
  {
    m_queueSize.fetch_add(1, std::memory_order_release);
    m_queueSize.notify_one();
  }

  template <typename F, typename... Args>
  auto executeWithFuture(F&& proc, Args&&... args)
  {
    using ret_t = decltype(std::invoke(std::forward<F>(proc), std::forward<Args>(args)...));
    std::packaged_task<ret_t(std::add_rvalue_reference_t<std::decay_t<Args>>...)> task{
      std::forward<F>(proc)};
    auto ret = task.get_future();
    auto capture = [task = std::move(task), ... args = std::forward<Args>(args)] mutable {
      std::invoke(std::move(task), std::move(args)...);
    };

    {
      std::scoped_lock lock{m_queueLock};
      m_queue.emplace_back(std::move(capture));
      m_queueSize.fetch_add(1, std::memory_order_relaxed);
    }
    m_queueSize.notify_one();

    return ret;
  }

  template <typename F, typename C, typename... Args>
  void executeWithCallback(F&& proc, C&& callback, Args&&... args)
  {
    using ret_t = decltype(std::invoke(std::forward<F>(proc), std::forward<Args>(args)...));
    std::packaged_task<ret_t(std::add_rvalue_reference_t<std::decay_t<Args>>...)> task{
      std::forward<F>(proc)};
    auto capture = [task = std::move(task),
                    cbk = std::forward<C>(callback),
                    ... args = std::forward<Args>(args)] mutable {
      auto fut = task.get_future();
      std::invoke(std::move(task), std::move(args)...);
      std::invoke(std::move(cbk), std::move(fut));
    };

    {
      std::scoped_lock lock{m_queueLock};
      m_queue.emplace_back(std::move(capture));
      m_queueSize.fetch_add(1, std::memory_order_relaxed);
    }
    m_queueSize.notify_one();
  }

  template <typename F, typename... Args>
  void executeDiscardResult(F&& proc, Args&&... args)
  {
    std::packaged_task<void(std::add_rvalue_reference_t<std::decay_t<Args>>...)> task{std::forward<F>(proc)};
    auto capture = [task = std::move(task), ... args = std::forward<Args>(args)] mutable {
      std::invoke(std::move(task), std::move(args)...);
    };

    {
      std::scoped_lock lock{m_queueLock};
      m_queue.emplace_back(std::move(capture));
      m_queueSize.fetch_add(1, std::memory_order_relaxed);
    }
    m_queueSize.notify_one();
  }

private:
  void worker()
  {
    queue_t queue;
    while (true) {
      if (queue.empty()) {
        m_queueSize.wait(0, std::memory_order_relaxed);
        {
          std::scoped_lock lock(m_queueLock);
          queue.splice(queue.begin(), m_queue);
          m_queueSize.fetch_sub(queue.size(), std::memory_order_relaxed);
        }
        if (queue.empty()) {
          break;
        }
      }

      std::invoke(std::move(queue.front()));
      queue.pop_front();
    }
  }

private:
  std::mutex m_queueLock;
  queue_t m_queue;
  std::atomic<size_t> m_queueSize = 0;
  std::jthread m_worker;
};

} // namespace openpressod

#endif // PROCEDURES_SERIAL_EXECUTOR_HPP