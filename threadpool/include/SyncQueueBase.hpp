// SyncQueue 公共基类（消除 4 个变体间的重复代码）
// 注意：gcc 4.9.2 不支持模板成员函数中的 lambda 谓词，
//       因此使用 while 循环替代 condition_variable::wait(pred)

#ifndef SYNC_QUEUE_BASE_HPP
#define SYNC_QUEUE_BASE_HPP

#include <deque>
#include <mutex>
#include <condition_variable>
#include <assert.h>
#include <chrono>
#include <thread>

namespace tulun { namespace detail {

// std::apply 的 C++14 实现（C++17 前无 std::apply）
template <class F, class Tuple, size_t... I>
auto apply_impl(F &&f, Tuple &&t, std::index_sequence<I...>)
    -> decltype(f(std::get<I>(std::forward<Tuple>(t))...))
{
    return f(std::get<I>(std::forward<Tuple>(t))...);
}



template <class T>
class SyncQueueBase
{
protected:
    std::deque<T> m_queue;
    mutable std::mutex m_mutex;
    std::condition_variable m_notEmpty;
    std::condition_variable m_notFull;
    std::condition_variable m_waitStop;
    int m_maxSize;
    int m_waitTime;
    bool m_needStop;
    int m_timeout;

    bool IsFull() const { return m_queue.size() >= static_cast<size_t>(m_maxSize); }
    bool IsEmpty() const { return m_queue.empty(); }

    template <class F>
    int Add(F &&task)
    {
        std::unique_lock<std::mutex> locker(m_mutex);

        if (m_timeout < 0)
        {
            while (!m_needStop && IsFull())
                m_notFull.wait(locker);
        }
        else if (m_timeout == 0)
        {
            if (IsFull())
            {
                if (m_needStop) return -2;
                return -1;
            }
        }
        else
        {
            while (!m_needStop && IsFull())
            {
                if (m_notFull.wait_for(locker, std::chrono::milliseconds(m_timeout)) == std::cv_status::timeout)
                {
                    if (m_needStop) return -2;
                    return -1;
                }
            }
        }

        if (m_needStop) return -2;
        m_queue.push_back(std::forward<F>(task));
        m_notEmpty.notify_one();
        return 0;
    }

    int PutImpl(T &&task)       { return Add(std::forward<T>(task)); }
    int PutImpl(const T &task)  { return Add(task); }

public:
    SyncQueueBase(int maxsize, int timeout = 0)
        : m_maxSize(maxsize), m_waitTime(50), m_needStop(false), m_timeout(timeout) {}

    ~SyncQueueBase()
    {
        if (!m_needStop) Stop();
    }

    void Stop()
    {
        {
            std::unique_lock<std::mutex> locker(m_mutex);
            m_needStop = true;
        }
        m_notEmpty.notify_all();
        m_notFull.notify_all();
    }

    void WaitQueueEmptyStopImpl()
    {
        std::unique_lock<std::mutex> locker(m_mutex);
        while (!m_queue.empty())
        {
            m_waitStop.wait_for(locker, std::chrono::milliseconds(m_waitTime));
        }
        m_needStop = true;
        m_notEmpty.notify_all();
        m_notFull.notify_all();
    }

    bool empty() const
    {
        std::unique_lock<std::mutex> locker(m_mutex);
        return m_queue.empty();
    }

    bool full() const
    {
        std::unique_lock<std::mutex> locker(m_mutex);
        return m_queue.size() >= static_cast<size_t>(m_maxSize);
    }

    size_t Size() const
    {
        std::unique_lock<std::mutex> locker(m_mutex);
        return m_queue.size();
    }

    size_t Count() const
    {
        std::unique_lock<std::mutex> locker(m_mutex);
        return m_queue.size();
    }
};

}} // namespace tulun::detail

#endif
