#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>
#include <condition_variable>
using namespace std;
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

int main()
{
    const int N = 5000;
    Barrier b(N);
    
    tulun::WorkStealingPool pool(5000, 16);
    cout << "Submitting " << N << " tasks with Barrier..." << endl;
    
    for (int i = 0; i < N; ++i)
    {
        pool.AddTask([&b]() { b.arrive(); });
    }
    
    cout << "Waiting on barrier..." << endl;
    b.wait();
    cout << "Barrier released! All tasks done." << endl;
    return 0;
}
