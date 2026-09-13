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
#include "WorkStealingPool.hpp"

class Barrier {
    mutex m_mtx; condition_variable m_cv; int m_count;
public:
    Barrier(int n) : m_count(n) {}
    void arrive() { lock_guard<mutex> lk(m_mtx); if (--m_count <= 0) m_cv.notify_all(); }
    void wait() { unique_lock<mutex> lk(m_mtx); m_cv.wait(lk, [this]() { return m_count <= 0; }); }
};
class Timer {
    using C = chrono::high_resolution_clock; C::time_point s;
public:
    void start() { s = C::now(); }
    double ms() const { return chrono::duration<double,milli>(C::now()-s).count(); }
};
thread_local mt19937 rng{random_device{}()};
void rs(int n) { vector<int> v(n); for(int i=0;i<n;++i) v[i]=uniform_int_distribution<int>(0,9999)(rng); sort(v.begin(),v.end()); }

int main() {
    const int N=5000, M=200;
    cout << "WorkStealingPool: " << N << " tasks..." << flush;
    Barrier b(N);
    tulun::WorkStealingPool pool(5000, thread::hardware_concurrency());
    Timer t; t.start();
    for(int i=0;i<N;++i) pool.AddTask([&](){ rs(M); b.arrive(); });
    b.wait();
    cout << " " << t.ms() << " ms OK" << endl;
    return 0;
}
