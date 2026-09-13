#include <iostream>
#include <iomanip>
#include <thread>
#include <atomic>
#include <vector>
#include <future>
#include <chrono>
using namespace std;

#include "WorkStealingPool.hpp"
#include "CachedThreadPool.hpp"
#include "FixedThreadPool.hpp"
#include "Timer.hpp"

atomic<unsigned long long> sink{0};

// 高精度计时
class Timer { using C=chrono::high_resolution_clock; C::time_point s;
public: void start(){s=C::now();} double ms(){return chrono::duration<double,milli>(C::now()-s).count();} };

// 工作负载
int heavy_cpu(int n) {
    volatile int sum=0;
    for(int i=0;i<n;++i) sum+=i%100;
    return sum;
}

template<class Pool>
void stress_submit(Pool& pool, const string& name, int tasks, int concurrency) {
    atomic<int> done{0};
    vector<thread> producers;
    int perProducer = tasks / concurrency;

    Timer t; t.start();
    for(int p=0; p<concurrency; ++p) {
        producers.emplace_back([&pool, &done, perProducer]() {
            for(int i=0; i<perProducer; ++i) {
                pool.submit(heavy_cpu, 1000);
                done.fetch_add(1, memory_order_relaxed);
            }
        });
    }
    for(auto& th: producers) th.join();
    double submitMs = t.ms();

    double rate = (done.load()*1000.0) / submitMs;
    cout << "  " << setw(20) << name
         << ": " << setw(8) << fixed << setprecision(0) << rate << " tasks/s"
         << "  (" << submitMs << " ms submit)" << endl;
}

template<class Pool>
void stress_addtask(Pool& pool, const string& name, int tasks, int concurrency) {
    atomic<int> done{0};
    vector<thread> producers;
    int perProducer = tasks / concurrency;

    Timer t; t.start();
    for(int p=0; p<concurrency; ++p) {
        producers.emplace_back([&pool, &done, perProducer]() {
            for(int i=0; i<perProducer; ++i) {
                pool.AddTask([&done]() {
                    heavy_cpu(500);
                    done.fetch_add(1, memory_order_relaxed);
                });
            }
        });
    }
    for(auto& th: producers) th.join();
    double submitMs = t.ms();

    // 等待完成
    while(done.load() < tasks) this_thread::sleep_for(chrono::milliseconds(10));
    double totalMs = t.ms();

    double rate = (tasks*1000.0) / submitMs;
    cout << "  " << setw(20) << name
         << ": " << setw(8) << fixed << setprecision(0) << rate << " tasks/s"
         << "  (submit=" << submitMs << "ms, total=" << totalMs << "ms)" << endl;
}

int main() {
    int hw = thread::hardware_concurrency();
    cout << "\n  ╔══════════════════════════════════════╗\n"
            "  ║  线程池 压力测试 (gcc 4.9, " << setw(2) << hw << "核) ║\n"
            "  ╚══════════════════════════════════════╝\n";

    // === 测试1: 大量任务提交吞吐量 ===
    {
        cout << "\n--- 测试1: 10万任务 submit 吞吐 (4生产者) ---" << endl;
        const int N=100000, CONC=4;

        { tulun::WorkStealingPool pool(N, hw);
          stress_submit(pool, "WorkStealingPool", N, CONC); }

        { tulun::CachedThreadPool pool(4, hw+2, 60000, N);
          stress_submit(pool, "CachedThreadPool", N, CONC); }

        { tulun::FixedThreadPool pool(N, hw);
          stress_submit(pool, "FixedThreadPool", N, CONC); }
    }

    // === 测试2: 多生产者 AddTask 并发 ===
    {
        cout << "\n--- 测试2: 2万任务 AddTask (8生产者并发) ---" << endl;
        const int N=20000, CONC=8;

        { tulun::WorkStealingPool pool(N, hw);
          stress_addtask(pool, "WorkStealingPool", N, CONC); }

        { tulun::CachedThreadPool pool(4, hw+2, 60000, N);
          stress_addtask(pool, "CachedThreadPool", N, CONC); }

        { tulun::FixedThreadPool pool(N, hw);
          stress_addtask(pool, "FixedThreadPool", N, CONC); }
    }

    // === 测试3: Timer 高频率触发 ===
    {
        cout << "\n--- 测试3: Timer 高频触发 (每50ms, 持续2秒) ---" << endl;
        tulun::TimerManager tm;
        atomic<int> count{0};
        tm.runEvery(50, [&count]() { count.fetch_add(1, memory_order_relaxed); });
        this_thread::sleep_for(chrono::seconds(2));
        tm.stop();
        cout << "  2秒内触发: " << count.load() << " 次 (预期~40)" << endl;
        cout << "  Timer " << (count.load() >= 35 ? "PASS" : "CHECK") << endl;
    }

    // === 测试4: Caller Runs 背压验证 ===
    {
        cout << "\n--- 测试4: Caller Runs — 小队列大量任务不丢 ---" << endl;
        tulun::FixedThreadPool pool(10, 2);  // 队列仅10, 2线程
        atomic<int> sum{0};
        const int N=1000;
        for(int i=0; i<N; ++i) {
            pool.AddTask([&sum, i]() {
                sum.fetch_add(i, memory_order_relaxed);
            });
        }
        while(sum.load() < (N-1)*N/2) {
            this_thread::sleep_for(chrono::milliseconds(10));
        }
        int expected = (N-1)*N/2;  // 0+1+2+...+999 = 499500
        cout << "  sum=" << sum.load() << " expected=" << expected
             << "  " << (sum.load()==expected ? "PASS" : "FAIL") << endl;
    }

    cout << "\n  ========== 压力测试完成 ==========\n" << endl;
    return 0;
}
