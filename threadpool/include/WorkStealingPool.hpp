#include <vector>
#include <thread>
#include <future>
#include <memory>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <assert.h>
#include <atomic>
#include <functional>

#include "SyncQueue_4.hpp"
#include "SyncQueueBase.hpp"  // detail::apply_impl

#ifndef WORK_STEALING_POOL_HPP
#define WORK_STEALING_POOL_HPP

namespace tulun
{
    using Task = std::function<void()>;

    class WorkStealingPool
    {
    private:
        const size_t m_numThreads;
        tulun::SyncQueue4<Task> m_queue;
        std::vector<std::shared_ptr<std::thread>> m_threadgroup;
        std::atomic<bool> m_running;
        std::once_flag m_flag;

    private:
        void Start(int numthreads)
        {
            m_running = true;
            m_threadgroup.reserve(numthreads);
            for (int i = 0; i < numthreads; ++i)
            {
                m_threadgroup.emplace_back(std::make_shared<std::thread>(&WorkStealingPool::RunInThread, this, i));
            }
        }
        void RunInThread(const int index)
        {
            int stealStart = index;
            while (m_running)
            {
                std::deque<Task> qutask;

                // 1. 阻塞等待自己的桶有任务
                int ret = m_queue.Task(&qutask, index);
                if (ret != 0) break; // 队列停止，退出

                // 2. 执行自己桶里的任务
                for (auto &task : qutask)
                {
                    if (task) task();
                }

                // 3. 执行完自己的任务后清空，避免窃取时重复执行
                qutask.clear();

                // 4. 尝试从别的桶窃取（非阻塞）
                while (m_running)
                {
                    // 先快速检查自己的桶
                    if (m_queue.TryTask(&qutask, index) == 0)
                    {
                        for (auto &task : qutask) { if (task) task(); }
                        qutask.clear();
                        break;
                    }

                    // 窃取其他桶
                    bool stole = false;
                    for (size_t k = 1; k < m_numThreads; ++k)
                    {
                        int victim = (stealStart + (int)k) % (int)m_numThreads;
                        if (victim == index) continue;
                        if (m_queue.TryTask(&qutask, victim) == 0)
                        {
                            for (auto &task : qutask) { if (task) task(); }
                            qutask.clear();
                            stole = true;
                            stealStart = victim;
                            break;
                        }
                    }
                    if (!stole) break;
                }
            }
        }

        void StopThreadGroup()
        {
            m_queue.Stop();
            m_running = false;

            for (auto &threadPtr : m_threadgroup)
            {
                if (threadPtr && threadPtr->joinable())
                {
                    threadPtr->join();
                }
            }
            m_threadgroup.clear();
        }

    public:
        WorkStealingPool(const int qusize = 100, const int numthreads = 8)
            : m_numThreads(numthreads), m_queue(numthreads, qusize), m_running(false)
        {
            Start(numthreads);
        }
        ~WorkStealingPool()
        {
            if (m_running)
            {
                Stop();
            }
        }
        void Stop()
        {
            StopThreadGroup();
        }

        template <typename F>
        void AddTask(F &&task)
        {
            Task wrapped = std::forward<F>(task);
            static std::atomic<int> s_dist{0};
            int index = s_dist.fetch_add(1, std::memory_order_relaxed) % m_numThreads;
            if (m_queue.Put(wrapped, index) != 0)
            {
                wrapped();  // Caller Runs
            }
        }

        // 注意：只有一个 AddTask 重载，避免歧义
        // submit 用于需要返回值的场景

        template <class Func, class... Args>
        auto submit(Func &&func, Args &&...args) -> std::future<decltype(func(args...))>
        {
            using RetType = decltype(func(args...));

            // 用 shared_ptr<function> 包装返回值（避免 gcc 4.9 tuple+index_sequence 兼容问题）
            auto fn = std::make_shared<std::function<RetType()>>(
                std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
            auto promise = std::make_shared<std::promise<RetType>>();
            auto result = promise->get_future();

            AddTask([fn, promise]() {
                try { promise->set_value((*fn)()); }
                catch (...) { promise->set_exception(std::current_exception()); }
            });

            return result;
        }
    };
}

#endif // WORK_STEALING_POOL_HPP