// WorkStealingPool 专用多桶队列
//   核心改进：每桶独立锁，消除全局锁竞争
//   窃取粒度：TryTask 取半数任务（而非整桶搬走）

#ifndef SYNC_QUEUE_4_HPP
#define SYNC_QUEUE_4_HPP

#include <deque>
#include <mutex>
#include <vector>
#include <condition_variable>
#include <atomic>
#include <cassert>
#include <algorithm>

namespace tulun
{
    const size_t MaxTaskCount4 = 64;

    template <class T>
    class SyncQueue4
    {
    private:
        struct Bucket
        {
            std::deque<T> tasks;
            mutable std::mutex mtx;
            std::condition_variable notEmpty;
        };

        std::vector<Bucket> m_buckets;
        size_t m_maxPerBucket;          // 每桶最大任务数
        std::atomic<bool> m_needStop{false};

    public:
        SyncQueue4(int bucketSize, int maxPerBucket = MaxTaskCount4, int /*timeout*/ = 0)
            : m_buckets(bucketSize), m_maxPerBucket(maxPerBucket)
        {
        }

        ~SyncQueue4()
        {
            if (!m_needStop) Stop();
        }

        // ========== 生产者 ==========

        int Put(const T &task, const int index)
        {
            auto &bk = m_buckets[index];
            std::unique_lock<std::mutex> lk(bk.mtx);

            if (m_needStop.load(std::memory_order_acquire)) return -2;
            if (bk.tasks.size() >= m_maxPerBucket) return -1; // Caller Runs

            bk.tasks.push_back(task);
            bk.notEmpty.notify_one();
            return 0;
        }

        int Put(T &&task, const int index)
        {
            auto &bk = m_buckets[index];
            std::unique_lock<std::mutex> lk(bk.mtx);

            if (m_needStop.load(std::memory_order_acquire)) return -2;
            if (bk.tasks.size() >= m_maxPerBucket) return -1;

            bk.tasks.push_back(std::move(task));
            bk.notEmpty.notify_one();
            return 0;
        }

        // ========== 消费者 ==========

        // 阻塞等待自己桶的任务（无限等待），只取一批（最多半数），
        // 让其他线程有机会窃取
        int Task(std::deque<T> *pdeq, const int index)
        {
            assert(pdeq != nullptr);
            auto &bk = m_buckets[index];
            std::unique_lock<std::mutex> lk(bk.mtx);

            while (!m_needStop.load(std::memory_order_acquire) && bk.tasks.empty())
                bk.notEmpty.wait(lk);

            if (m_needStop.load(std::memory_order_acquire)) return -2;

            // 只取一批（最多半数，最少 1 个），留下空间给窃取
            size_t batch = std::max(size_t(1), bk.tasks.size() / 2);
            auto it = bk.tasks.begin() + (bk.tasks.size() - batch);
            pdeq->insert(pdeq->end(),
                         std::make_move_iterator(it),
                         std::make_move_iterator(bk.tasks.end()));
            bk.tasks.erase(it, bk.tasks.end());
            return 0;
        }

        // 非阻塞窃取：取走约半数任务（而非整桶）
        int TryTask(std::deque<T> *pdeq, const int victimIndex)
        {
            assert(pdeq != nullptr);
            auto &bk = m_buckets[victimIndex];
            std::unique_lock<std::mutex> lk(bk.mtx);

            if (bk.tasks.empty()) return -1;

            // 窃取一半（至少 1 个）：受害者保留局部性，盗贼也有活干
            size_t stealCount = std::max(size_t(1), bk.tasks.size() / 2);
            auto it = bk.tasks.begin() + (bk.tasks.size() - stealCount);
            pdeq->insert(pdeq->end(),
                         std::make_move_iterator(it),
                         std::make_move_iterator(bk.tasks.end()));
            bk.tasks.erase(it, bk.tasks.end());
            return 0;
        }

        // 取单个任务（阻塞等待，带超时）
        int Task(T *ps, const int index)
        {
            assert(ps != nullptr);
            auto &bk = m_buckets[index];
            std::unique_lock<std::mutex> lk(bk.mtx);

            while (!m_needStop.load(std::memory_order_acquire) && bk.tasks.empty())
                bk.notEmpty.wait(lk);

            if (m_needStop.load(std::memory_order_acquire)) return -2;

            *ps = std::move(bk.tasks.front());
            bk.tasks.pop_front();
            return 0;
        }

        // ========== 生命周期 ==========

        void Stop()
        {
            m_needStop.store(true, std::memory_order_release);
            for (auto &bk : m_buckets)
            {
                std::unique_lock<std::mutex> lk(bk.mtx);
                bk.notEmpty.notify_all();
            }
        }

        // 查询（无锁快照，用于调试）
        size_t Size(const int index) const
        {
            std::unique_lock<std::mutex> lk(m_buckets[index].mtx);
            return m_buckets[index].tasks.size();
        }

        size_t TotalTaskNum() const
        {
            size_t total = 0;
            for (size_t i = 0; i < m_buckets.size(); ++i)
            {
                std::unique_lock<std::mutex> lk(m_buckets[i].mtx);
                total += m_buckets[i].tasks.size();
            }
            return total;
        }
    };

} // namespace tulun

#endif
