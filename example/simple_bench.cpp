#include <iostream>
#include <thread>
#include <atomic>
#include <vector>
#include <algorithm>
#include <random>
#include <chrono>
#include <mutex>
#include <condition_variable>
using namespace std;

#include "FixedThreadPool.hpp"
#include "WorkStealingPool.hpp"

class Barrier
{
    mutex m_mtx;
    condition_variable m_cv;
    int m_count;
public:
    Barrier(int n) : m_count(n) {}
    void arrive() {
        lock_guard<mutex> lk(m_mtx);
        if (--m_count <= 0) m_cv.notify_all();
    }
    void wait() {
        unique_lock<mutex> lk(m_mtx);
        m_cv.wait(lk, [this]() { return m_count <= 0; });
    }
};

class Timer
{
    using Clock = chrono::high_resolution_clock;
    Clock::time_point m_start;
public:
    void start() { m_start = Clock::now(); }
    double elapsed_ms() const {
        return chrono::duration<double, milli>(Clock::now() - m_start).count();
    }
};

thread_local mt19937 g_rng{random_device{}()};

void random_sort(int n)
{
    vector<int> v(n);
    for (int i = 0; i < n; ++i)
        v[i] = uniform_int_distribution<int>(0, 9999)(g_rng);
    sort(v.begin(), v.end());
}

int main()
{
    const int N = 5000;
    const int M = 200;
    int hw = thread::hardware_concurrency();
    cout << "Hardware threads: " << hw << endl;
    cout << "Tasks: " << N << ", sort size: " << M << endl;
    cout << "========================================" << endl;

    // ---- Test FixedThreadPool ----
    cout << "\n[1/2] Testing FixedThreadPool..." << flush;
    {
        Barrier b(N);
        tulun::FixedThreadPool pool(5000, hw);
        Timer tb;
        tb.start();
        for (int i = 0; i < N; ++i)
            pool.AddTask([&]() { random_sort(M); b.arrive(); });
        b.wait();
        double ms = tb.elapsed_ms();
        cout << " done in " << ms << " ms" << endl;
    }
    cout << "FixedThreadPool destroyed" << endl;

    // ---- Test WorkStealingPool ----
    cout << "\n[2/2] Testing WorkStealingPool..." << flush;
    {
        Barrier b(N);
        tulun::WorkStealingPool pool(5000, hw);
        Timer tb;
        tb.start();
        for (int i = 0; i < N; ++i)
            pool.AddTask([&]() { random_sort(M); b.arrive(); });
        b.wait();
        double ms = tb.elapsed_ms();
        cout << " done in " << ms << " ms" << endl;
    }
    cout << "WorkStealingPool destroyed" << endl;

    cout << "\n=== ALL TESTS PASSED ===" << endl;
    return 0;
}
