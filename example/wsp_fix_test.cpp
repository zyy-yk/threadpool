#include <iostream>
#include <thread>
#include <atomic>
#include <vector>
#include <future>
using namespace std;
#include "WorkStealingPool.hpp"

int main() {
    cout << "WSP submit test (4 threads)..." << endl;
    { tulun::WorkStealingPool p(2000, 4);
        vector<future<int>> futs;
        for (int i = 0; i < 50; ++i)
            futs.push_back(p.submit([](int x) { return x * x; }, i));
        int sum = 0;
        for (auto &f : futs) sum += f.get();
        cout << "sum=" << sum << " expect=40425 -> " << (sum==40425?"PASS":"FAIL") << endl;
    }
    cout << "Destroy OK" << endl;
    return 0;
}
