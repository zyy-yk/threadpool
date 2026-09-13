#include <iostream>
#include <thread>
#include <atomic>
#include <vector>
#include <future>
using namespace std;
#include "WorkStealingPool.hpp"

int main() {
    cout << "hw=" << thread::hardware_concurrency() << endl;
    
    try {
        cout << "Creating WSP..." << flush;
        tulun::WorkStealingPool pool(2000, 4);
        cout << "OK" << endl;
        
        cout << "submit 1 task..." << flush;
        auto f1 = pool.submit([](int x){ return x*2; }, 21);
        cout << "result=" << f1.get() << flush;
        cout << " OK" << endl;
        
        cout << "submit 5 tasks..." << flush;
        vector<future<int>> futs;
        for (int i = 0; i < 5; ++i)
            futs.push_back(pool.submit([](int x){ return x*x; }, i));
        int sum = 0;
        for (auto &f : futs) {
            try { sum += f.get(); }
            catch (exception &e) { cerr << "FUTURE_ERR: " << e.what() << endl; }
        }
        cout << "sum=" << sum << flush;
        cout << " OK" << endl;
        
    } catch (exception &e) {
        cerr << "EXCEPTION: " << e.what() << endl;
        return 1;
    }
    cout << "ALL OK" << endl;
    return 0;
}
