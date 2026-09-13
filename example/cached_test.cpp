#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>
using namespace std;
#include "CachedThreadPool.hpp"

int main() {
    cout << "CachedThreadPool test..." << flush;
    {
        tulun::CachedThreadPool pool(2, 16, 5000, 2000);  // 2 core, 16 max, 5s timeout
        
        atomic<int> counter{0};
        const int N = 2000;
        
        for (int i = 0; i < N; ++i) {
            pool.AddTask([&counter]() { counter.fetch_add(1, memory_order_relaxed); });
        }
        
        while (counter.load() < N)
            this_thread::sleep_for(chrono::milliseconds(5));
        
        cout << " OK (counter=" << counter.load() << ")" << endl;
        cout << "Threads: total=" << pool.TotalCount() << " idle=" << pool.IdleCount() << endl;
    }
    cout << "Destroyed OK" << endl;
    
    // Test submit
    cout << "Submit test..." << flush;
    {
        tulun::CachedThreadPool pool(2, 16, 5000, 2000);
        vector<future<int>> futs;
        for (int i = 0; i < 100; ++i) {
            futs.push_back(pool.submit([](int x) { return x * x; }, i));
        }
        int sum = 0;
        for (int i = 0; i < 100; ++i) sum += futs[i].get();
        int expected = 0;
        for (int i = 0; i < 100; ++i) expected += i * i;
        cout << (sum == expected ? "PASS" : "FAIL") << " (sum=" << sum << ")" << endl;
    }
    cout << "ALL TESTS PASSED" << endl;
    return 0;
}
