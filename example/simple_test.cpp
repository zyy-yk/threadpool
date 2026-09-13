#include <iostream>
#include <thread>
#include <functional>
#include <atomic>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <cassert>

#include "FixedThreadPool.hpp"
#include "Timestamp.hpp"

// 简单的 Barrier 替代 C++20 std::latch
class SimpleLatch
{
private:
    std::mutex m_mutex;
    std::condition_variable m_cv;
    int m_count;

public:
    SimpleLatch(int count) : m_count(count) {}

    void count_down()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        --m_count;
        if (m_count <= 0)
        {
            m_cv.notify_all();
        }
    }

    void wait()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [this]() { return m_count <= 0; });
    }
};

// ==================== 测试 1: 基本 AddTask ====================
void test_basic_addtask()
{
    std::cout << "=== 测试 1: 基本 AddTask ===" << std::endl;

    tulun::FixedThreadPool pool(2000, 4);
    std::atomic<int> counter(0);
    const int N = 1000;

    for (int i = 0; i < N; ++i)
    {
        pool.AddTask([&counter]() {
            ++counter;
        });
    }

    // 等待任务完成
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::cout << "counter = " << counter.load() << " (expected: " << N << ")" << std::endl;
    std::cout << (counter.load() == N ? "✅ PASS" : "❌ FAIL") << std::endl;
    std::cout << std::endl;
}

// ==================== 测试 2: submit 返回值 ====================
void test_submit_return()
{
    std::cout << "=== 测试 2: submit 返回值 ===" << std::endl;

    tulun::FixedThreadPool pool(2000, 4);
    const int N = 100;
    std::vector<std::future<int>> futures;

    for (int i = 0; i < N; ++i)
    {
        futures.push_back(pool.submit([](int x) -> int {
            return x * x;
        }, i));
    }

    bool all_pass = true;
    for (int i = 0; i < N; ++i)
    {
        int result = futures[i].get();
        if (result != i * i)
        {
            std::cout << "Mismatch at " << i << ": got " << result << ", expected " << i * i << std::endl;
            all_pass = false;
        }
    }

    std::cout << (all_pass ? "✅ PASS" : "❌ FAIL") << std::endl;
    std::cout << std::endl;
}

// ==================== 测试 3: 多线程生产者 ====================
void test_multi_producer()
{
    std::cout << "=== 测试 3: 多线程生产者 ===" << std::endl;

    tulun::FixedThreadPool pool(2000, 4);
    std::atomic<int> counter(0);
    const int TASKS_PER_THREAD = 500;
    const int NUM_PRODUCERS = 5;

    auto producer = [&pool, &counter, TASKS_PER_THREAD]() {
        for (int i = 0; i < TASKS_PER_THREAD; ++i)
        {
            pool.AddTask([&counter]() {
                ++counter;
            });
        }
    };

    std::vector<std::thread> producers;
    for (int i = 0; i < NUM_PRODUCERS; ++i)
    {
        producers.emplace_back(producer);
    }
    for (auto &t : producers)
    {
        t.join();
    }

    // 等待任务完成
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    int expected = TASKS_PER_THREAD * NUM_PRODUCERS;
    std::cout << "counter = " << counter.load() << " (expected: " << expected << ")" << std::endl;
    std::cout << (counter.load() == expected ? "✅ PASS" : "❌ FAIL") << std::endl;
    std::cout << std::endl;
}

// ==================== 测试 4: SyncQueue 线程安全 ====================
void test_syncqueue()
{
    std::cout << "=== 测试 4: SyncQueue 基本操作 ===" << std::endl;

    tulun::SyncQueue<int> q(10, 100);

    // 测试 Put/Task
    bool all_pass = true;

    for (int i = 0; i < 5; ++i)
    {
        if (q.Put(i) != 0)
        {
            std::cout << "Put failed at " << i << std::endl;
            all_pass = false;
        }
    }

    if (q.Size() != 5)
    {
        std::cout << "Size mismatch: " << q.Size() << " != 5" << std::endl;
        all_pass = false;
    }

    int val;
    q.Task(&val);
    if (val != 0)
    {
        std::cout << "Task value mismatch: " << val << " != 0" << std::endl;
        all_pass = false;
    }

    if (q.Size() != 4)
    {
        std::cout << "Size after Task mismatch: " << q.Size() << " != 4" << std::endl;
        all_pass = false;
    }

    q.Stop();

    std::cout << (all_pass ? "✅ PASS" : "❌ FAIL") << std::endl;
    std::cout << std::endl;
}

// ==================== 测试 5: 队列满时的行为 ====================
void test_queue_full()
{
    std::cout << "=== 测试 5: 队列满时 submit 同步执行 (不丢失任务) ===" << std::endl;

    // 创建小队列 (size=2) 和 1 个工作线程 (让它处理慢一点)
    tulun::FixedThreadPool pool(2, 1);
    std::atomic<int> counter(0);
    const int N = 100;

    // 提交大量任务，队列必然会满
    std::vector<std::future<int>> futures;
    for (int i = 0; i < N; ++i)
    {
        futures.push_back(pool.submit([i, &counter]() -> int {
            ++counter;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            return i;
        }));
    }

    // 等待所有 future
    int sum = 0;
    for (auto &f : futures)
    {
        sum += f.get();  // 如果任务丢失，这里会死锁
    }

    int expected_sum = (N - 1) * N / 2;  // 0 + 1 + 2 + ... + 99 = 4950
    std::cout << "sum = " << sum << " (expected: " << expected_sum << ")" << std::endl;
    std::cout << "counter = " << counter.load() << " (expected: " << N << ")" << std::endl;

    bool pass = (sum == expected_sum) && (counter.load() == N);
    std::cout << (pass ? "✅ PASS" : "❌ FAIL") << std::endl;
    std::cout << std::endl;
}

// ==================== 测试 6: 线程池停止后不再处理任务 ====================
void test_stop()
{
    std::cout << "=== 测试 6: Stop 后不再处理新任务 ===" << std::endl;

    tulun::FixedThreadPool pool(2000, 4);
    std::atomic<int> counter(0);

    pool.AddTask([&counter]() { ++counter; });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    pool.Stop();

    int after_stop = counter.load();
    std::cout << "Tasks completed before stop: " << after_stop << std::endl;

    // Stop 后再提交（会被同步执行）
    pool.AddTask([&counter]() { ++counter; });
    int after_add = counter.load();
    std::cout << "Tasks after stop+add: " << after_add << " (should be +1)" << std::endl;

    std::cout << (after_add == after_stop + 1 ? "✅ PASS" : "❌ FAIL") << std::endl;
    std::cout << std::endl;
}

int main()
{
    std::cout << "========== 线程池单元测试 ==========" << std::endl;
    std::cout << "硬件线程数: " << std::thread::hardware_concurrency() << std::endl;
    std::cout << std::endl;

    test_syncqueue();
    test_basic_addtask();
    test_submit_return();
    test_multi_producer();
    test_queue_full();
    test_stop();

    std::cout << "========== 全部测试完成 ==========" << std::endl;
    return 0;
}
