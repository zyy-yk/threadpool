// 固定线程池专用 SyncQueue（继承自公共基类）
#ifndef SYNC_QUEUE_2_HPP
#define SYNC_QUEUE_2_HPP

#include "SyncQueueBase.hpp"

namespace tulun
{
    const size_t MaxTaskCount2 = 500;

    template <class T>
    class SyncQueue2 : public detail::SyncQueueBase<T>
    {
        using Base = detail::SyncQueueBase<T>;

    public:
        using Base::Stop;
        using Base::empty;
        using Base::full;
        using Base::Size;
        using Base::Count;

        SyncQueue2(int maxsize = MaxTaskCount2, int timeout = 0)
            : Base(maxsize, timeout) {}

        int Put(const T &task)  { return Base::PutImpl(task); }
        int Put(T &&task)       { return Base::PutImpl(std::forward<T>(task)); }

        int Task(std::deque<T> *pdeq)
        {
            assert(pdeq != nullptr);
            std::unique_lock<std::mutex> locker(Base::m_mutex);
            while (!Base::m_needStop && Base::IsEmpty())
                Base::m_notEmpty.wait(locker);
            if (Base::m_needStop) return -2;
            *pdeq = std::move(Base::m_queue);
            Base::m_notFull.notify_one();
            Base::m_waitStop.notify_one();
            return 0;
        }

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

        void WaitQueueEmptyStop() { Base::WaitQueueEmptyStopImpl(); }
    };

} // namespace tulun
#endif
