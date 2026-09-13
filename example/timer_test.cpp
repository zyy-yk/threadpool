#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
using namespace std;
#include "Timer.hpp"

int main() {
    cout << "TimerManager test..." << endl;
    
    tulun::TimerManager tm;
    atomic<int> count{0};
    
    // 一次性定时器：500ms 后执行
    tm.runAfter(500, [&count]() {
        cout << "  [1] runAfter(500ms) fired, count=" << ++count << endl;
    });
    
    // 周期性定时器：每 300ms 执行
    int tid = tm.runEvery(300, [&count]() {
        cout << "  [2] runEvery(300ms) fired, count=" << ++count << endl;
    });
    
    // 等 2 秒看结果
    this_thread::sleep_for(chrono::seconds(2));
    
    cout << "Final count: " << count.load() << " (expected: 1 + ~6 = 7)" << endl;
    cout << "Pending timers: " << tm.pendingCount() << endl;
    tm.stop();
    cout << "Timer test PASSED" << endl;
    return 0;
}
