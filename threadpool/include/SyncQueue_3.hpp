// 缓存线程池专用 SyncQueue（继承自公共基类）
#ifndef SYNC_QUEUE_3_HPP
#define SYNC_QUEUE_3_HPP

#include "SyncQueueBase.hpp"

namespace tulun
{
    const size_t MaxTaskCount3 = 64;

    template <class T>
    class SyncQueue3 : public detail::SyncQueueBase<T>
    {
        using Base = detail::SyncQueueBase<T>;

    public:
        using Base::Stop;
        using Base::empty;
        using Base::full;
        using Base::Size;
        using Base::Count;

        SyncQueue3(int maxsize = MaxTaskCount3, int timeout = 0)
            : Base(maxsize, timeout) {}

        int Put(const T &task)  { return Base::PutImpl(task); }
        int Put(T &&task)       { return Base::PutImpl(std::forward<T>(task)); }

        int Task(T *ps)
        {
            assert(ps != nullptr);
            std::unique_lock<std::mutex> locker(Base::m_mutex);
            while (!Base::m_needStop && Base::IsEmpty())
                Base::m_notEmpty.wait(locker);
            if (Base::m_needStop) return -2;
            *ps = Base::m_queue.front();
            Base::m_queue.pop_front();
            Base::m_notFull.notify_one();
            Base::m_waitStop.notify_one();
            return 0;
        }

        int TaskWithTimeout(T *ps, int timeoutMs)
        {
            assert(ps != nullptr);
            std::unique_lock<std::mutex> locker(Base::m_mutex);
            auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
            while (!Base::m_needStop && Base::IsEmpty())
            {
                auto remaining = deadline - std::chrono::steady_clock::now();
                if (remaining <= std::chrono::milliseconds(0))
                {
                    if (Base::m_needStop) return -2;
                    return -1;
                }
                Base::m_notEmpty.wait_for(locker, remaining);
            }
            if (Base::m_needStop) return -2;
            *ps = Base::m_queue.front();
            Base::m_queue.pop_front();
            Base::m_notFull.notify_one();
            Base::m_waitStop.notify_one();
            return 0;
        }

        void WaitQueueEmptyStop() { Base::WaitQueueEmptyStopImpl(); }
    };

} // namespace tulun
#endif
