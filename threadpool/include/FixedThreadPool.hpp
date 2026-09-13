// 固定线程池：固定数量工作线程 + 共享任务队列
//   使用 Caller Runs 策略：队列满时提交线程自行执行任务，形成自然背压

#ifndef FIXED_THREAD_POOL_HPP
#define FIXED_THREAD_POOL_HPP

#include "SyncQueue_2.hpp"

#include <thread>
#include <functional>
#include <list>
#include <memory>
#include <atomic>
#include <future>

namespace tulun
{
    using TaskType = std::function<void(void)>;

    class FixedThreadPool
    {
    private:
        tulun::SyncQueue2<TaskType> m_queue;
        std::list<std::shared_ptr<std::thread>> m_threadgroup;
        std::atomic_bool m_running;
        std::once_flag m_flag;

        void Start(int numThreads)
        {
            m_running = true;
            for (int i = 0; i < numThreads; ++i)
            {
                m_threadgroup.push_back(std::make_shared<std::thread>(&FixedThreadPool::RunThread, this));
            }
        }

        void RunThread()
        {
            while (m_running)
            {
                TaskType task;
                int ret = m_queue.Task(&task);
                if (ret != 0) break;            // 队列停止，退出
                if (task) { task(); }
            }
        }

        void StopThreadGroup()
        {
            m_queue.WaitQueueEmptyStop();
            m_running = false;

            for (auto &tha : m_threadgroup)
            {
                tha->join();
            }
            m_threadgroup.clear();
        }

    public:
        FixedThreadPool(const size_t taskquesize = 2000,
                        int numthreads = std::thread::hardware_concurrency())
            : m_queue(static_cast<int>(taskquesize)),
              m_running(false)
        {
            Start(numthreads);
        }

        ~FixedThreadPool()
        {
            if (m_running) Stop();
        }

        void Stop()
        {
            std::call_once(m_flag, [this]() -> void { StopThreadGroup(); });
        }

        template <typename F>
        void AddTask(F &&task)
        {
            TaskType wrapped = std::forward<F>(task);
            if (m_queue.Put(wrapped) != 0)
            {
                wrapped();  // Caller Runs
            }
        }

        template <class Func, class... Args>
        auto submit(Func &&func, Args &&... args) -> std::future<decltype(func(args...))>
        {
            using RetType = decltype(func(args...));

            // 用 lambda + promise 替代 packaged_task，避免 gcc 4.9 的
            // tuple/index_sequence 兼容问题
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

} // namespace tulun

#endif
