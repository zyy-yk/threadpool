#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
using namespace std;
#include "FixedThreadPool.hpp"
#include "CachedThreadPool.hpp"

int main() {
    atomic<int> c{0};
    auto run = [&c](auto& pool, const string& name, int n) {
        c=0; for(int i=0;i<n;++i) pool.AddTask([&c](){c.fetch_add(1,memory_order_relaxed);});
        while(c.load()<n) this_thread::sleep_for(chrono::milliseconds(5));
        cout << "  " << name << ": OK (c=" << c.load() << ")" << endl;
    };

    cout << "Test 1: Cached first, Fixed second..." << endl;
    {tulun::CachedThreadPool cp(2,16,5000,2000); run(cp,"Cached",500);}
    {tulun::FixedThreadPool fp(2000,4);           run(fp,"Fixed ",500);}
    cout << "PASS" << endl;

    cout << "Test 2: Fixed first, Cached second..." << endl;
    {tulun::FixedThreadPool fp(2000,4);           run(fp,"Fixed ",500);}
    {tulun::CachedThreadPool cp(2,16,5000,2000); run(cp,"Cached",500);}
    cout << "PASS" << endl;

    cout << "ALL ORDER TESTS PASSED" << endl;
    return 0;
}
