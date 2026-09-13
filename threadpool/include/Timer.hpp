#ifndef TIMER_HPP
#define TIMER_HPP

#include <functional>
#include <chrono>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <queue>

namespace tulun
{
    using TimerCallback = std::function<void()>;

    // 跨平台定时器管理器
    //   内部用 std::chrono + 优先队列 + 后台线程
    class TimerManager
    {
    public:
        using TimePoint = std::chrono::steady_clock::time_point;
        using Duration  = std::chrono::milliseconds;

    private:
        struct Entry
        {
            int id;
            TimePoint nextFire;
            Duration interval;  // 0 = 一次性
            TimerCallback callback;
            bool operator<(const Entry &o) const { return nextFire > o.nextFire; }
        };

        mutable std::mutex m_mutex;
        std::condition_variable m_cv;
        std::priority_queue<Entry> m_timers;
        std::thread m_worker;
        std::atomic<bool> m_running{false};
        std::atomic<int>  m_nextId{1};

        void loop()
        {
            while (m_running.load(std::memory_order_relaxed))
            {
                std::unique_lock<std::mutex> lk(m_mutex);

                if (m_timers.empty())
                {
                    m_cv.wait(lk, [this]() {
                        return !m_running.load(std::memory_order_relaxed) || !m_timers.empty();
                    });
                    if (!m_running) break;
                    continue;
                }

                auto now = std::chrono::steady_clock::now();
                auto top = m_timers.top();

                if (top.nextFire <= now)
                {
                    m_timers.pop();
                    if (top.interval.count() > 0)
                    {
                        top.nextFire = now + top.interval;
                        m_timers.push(top);
                    }
                    lk.unlock();
                    if (top.callback) top.callback();
                }
                else
                {
                    m_cv.wait_for(lk, top.nextFire - now, [this]() {
                        return !m_running.load(std::memory_order_relaxed);
                    });
                }
            }
        }

    public:
        TimerManager()
        {
            m_running = true;
            m_worker = std::thread(&TimerManager::loop, this);
        }

        ~TimerManager() { stop(); }

        void stop()
        {
            if (!m_running.exchange(false)) return;
            m_cv.notify_all();
            if (m_worker.joinable()) m_worker.join();
        }

        int runAt(TimePoint when, TimerCallback cb)
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            int id = m_nextId.fetch_add(1, std::memory_order_relaxed);
            m_timers.push({id, when, Duration(0), std::move(cb)});
            m_cv.notify_one();
            return id;
        }

        int runAfter(int delayMs, TimerCallback cb)
        {
            return runAt(std::chrono::steady_clock::now() + std::chrono::milliseconds(delayMs),
                         std::move(cb));
        }

        int runEvery(int intervalMs, TimerCallback cb)
        {
            if (intervalMs <= 0) return -1;
            std::lock_guard<std::mutex> lk(m_mutex);
            int id = m_nextId.fetch_add(1, std::memory_order_relaxed);
            m_timers.push({id,
                           std::chrono::steady_clock::now() + std::chrono::milliseconds(intervalMs),
                           std::chrono::milliseconds(intervalMs),
                           std::move(cb)});
            m_cv.notify_one();
            return id;
        }

        size_t pendingCount() const
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            return m_timers.size();
        }
    };

} // namespace tulun

#endif
