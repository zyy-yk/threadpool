#ifndef CACHED_THREAD_POOL_HPP
#define CACHED_THREAD_POOL_HPP

#include "SyncQueue_3.hpp"

#include <thread>
#include <functional>
#include <list>
#include <memory>
#include <atomic>
#include <future>
#include <mutex>
#include <condition_variable>
#include <chrono>

namespace tulun
{
    using TaskType = std::function<void(void)>;
    static const size_t DefaultCoreThreads = 4;

    // 缓存式线程池：核心线程常驻，额外线程按需创建，空闲超时自动回收
    class CachedThreadPool
    {
    private:
        tulun::SyncQueue3<TaskType> m_queue;

        mutable std::mutex m_mgmtMutex;
        std::list<std::shared_ptr<std::thread>> m_threads; // 管理所有线程（join 用）

        std::atomic<int> m_idleCount{0};
        std::atomic<int> m_totalCount{0};
        int m_coreThreads;
        int m_maxThreads;
        int m_keepAliveMs;

        std::atomic<bool> m_running{false};
        std::once_flag m_stopFlag;

        void CoreWorker()
        {
            m_idleCount.fetch_add(1, std::memory_order_release);
            m_totalCount.fetch_add(1, std::memory_order_release);

            while (m_running.load(std::memory_order_acquire))
            {
                TaskType task;
                int ret = m_queue.Task(&task);  // 无限等待
                if (ret != 0) break;            // 队列停止

                m_idleCount.fetch_sub(1, std::memory_order_release);
                if (task) task();
                m_idleCount.fetch_add(1, std::memory_order_release);
            }

            m_totalCount.fetch_sub(1, std::memory_order_release);
            m_idleCount.fetch_sub(1, std::memory_order_release);
        }

        void ExtraWorker()
        {
            m_idleCount.fetch_add(1, std::memory_order_release);
            m_totalCount.fetch_add(1, std::memory_order_release);

            while (m_running.load(std::memory_order_acquire))
            {
                TaskType task;
                int ret = m_queue.TaskWithTimeout(&task, m_keepAliveMs);
                if (ret == -2) break;       // 队列停止
                if (ret == -1)               // 空闲超时
                {
                    if (m_totalCount.load(std::memory_order_relaxed) > m_coreThreads)
                        break;
                    continue;
                }

                m_idleCount.fetch_sub(1, std::memory_order_release);
                if (task) task();
                m_idleCount.fetch_add(1, std::memory_order_release);
            }

            m_totalCount.fetch_sub(1, std::memory_order_release);
            m_idleCount.fetch_sub(1, std::memory_order_release);
        }

        void Start(int coreThreads)
        {
            m_running.store(true, std::memory_order_release);
            std::unique_lock<std::mutex> lk(m_mgmtMutex);
            for (int i = 0; i < coreThreads; ++i)
            {
                auto t = std::make_shared<std::thread>(&CachedThreadPool::CoreWorker, this);
                m_threads.push_back(t);  // 保存以便 join
            }
            // 等待核心线程就绪
            while (m_idleCount.load(std::memory_order_acquire) < coreThreads)
                std::this_thread::yield();
        }

        void TryAddExtraThread()
        {
            if (m_totalCount.load(std::memory_order_relaxed) >= m_maxThreads)
                return;

            std::unique_lock<std::mutex> lk(m_mgmtMutex);
            if (m_totalCount.load(std::memory_order_relaxed) >= m_maxThreads)
                return;

            auto t = std::make_shared<std::thread>(&CachedThreadPool::ExtraWorker, this);
            m_threads.push_back(t);
        }

    public:
        CachedThreadPool(int coreThreads = DefaultCoreThreads,
                         int maxThreads = 0,
                         int keepAliveMs = 60000,
                         int queueSize = MaxTaskCount3)
            : m_queue(queueSize, 0),
              m_coreThreads(coreThreads),
              m_maxThreads(maxThreads > 0 ? maxThreads
                                          : (int)std::thread::hardware_concurrency() + 2),
              m_keepAliveMs(keepAliveMs)
        {
            if (m_maxThreads < m_coreThreads)
                m_maxThreads = m_coreThreads;
            Start(coreThreads);
        }

        ~CachedThreadPool()
        {
            Stop();
        }

        void Stop()
        {
            std::call_once(m_stopFlag, [this]() {
                // 1. 先停止队列和运行标志
                m_queue.Stop();
                m_running.store(false, std::memory_order_release);

                // 2. join 所有线程（不再依赖超时）
                std::unique_lock<std::mutex> lk(m_mgmtMutex);
                for (auto &t : m_threads)
                {
                    if (t && t->joinable())
                    {
                        lk.unlock();
                        t->join();
                        lk.lock();
                    }
                }
                m_threads.clear();
            });
        }

        // ---------- 提交任务 ----------

        template <typename F>
        void AddTask(F &&task)
        {
            TaskType wrapped = std::forward<F>(task);
            if (m_queue.Put(wrapped) != 0)
            {
                wrapped();  // Caller Runs
                return;
            }
            if (m_idleCount.load(std::memory_order_acquire) == 0)
                TryAddExtraThread();
        }

        template <typename F, typename... Args>
        auto submit(F &&f, Args &&...args) -> std::future<decltype(f(args...))>
        {
            using RetType = decltype(f(args...));
            auto fn = std::make_shared<std::function<RetType()>>(
                std::bind(std::forward<F>(f), std::forward<Args>(args)...));
            auto promise = std::make_shared<std::promise<RetType>>();
            auto result = promise->get_future();

            TaskType wrapper = [fn, promise]() {
                try { promise->set_value((*fn)()); }
                catch (...) { promise->set_exception(std::current_exception()); }
            };

            if (m_queue.Put(std::move(wrapper)) != 0)
            {
                wrapper();  // Caller Runs（或直接执行 fn+promise）
                return result;
            }
            if (m_idleCount.load(std::memory_order_acquire) == 0)
                TryAddExtraThread();
            return result;
        }

        int IdleCount()  const { return m_idleCount.load(); }
        int TotalCount() const { return m_totalCount.load(); }
        int CoreCount()  const { return m_coreThreads; }
    };

} // namespace tulun

#endif
