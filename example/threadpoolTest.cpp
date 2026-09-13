#include <iostream>
#include <thread>
#include <functional>
#include <atomic>
#include <vector>
#include <latch>
#include <cstdlib>

#include "FixedThreadPool.hpp"
#include "Timestamp.hpp"

std::atomic<int> ac(0);

tulun::FixedThreadPool mypool(2000, std::thread::hardware_concurrency());
const int n = 10000;
const int m = 1000;
std::latch mylat(n);
std::vector<std::vector<int>> ivec1(n);
std::vector<std::vector<int>> ivec2(n);

void RandVec1(std::vector<int> *pvec)
{
    assert(pvec != nullptr);
    for (int i = 0; i < m; ++i)
    {
        pvec->push_back(rand() % 10000);
    }
}

void test1()
{
    std::cout << "不使用线程池" << std::endl;
    tulun::Timestamp begin, end;
    begin.now();
    for (int i = 0; i < n; ++i)
    {
        RandVec1(&ivec1[i]);
    }
    end.now();
    std::cout << tulun::diffMicro(end, begin) << std::endl;
    std::cout << "\n===================================================\n"
              << std::endl;
}

void RandVec2(std::vector<int> *pvec)
{
    assert(pvec != nullptr);
    for (int i = 0; i < m; ++i)
    {
        pvec->push_back(rand() % 10000);
    }
    mylat.count_down();
}

void test2()
{
    std::cout << "使用线程池" << std::endl;
    tulun::Timestamp begin, end;
    begin.now();
    for (int i = 0; i < n; ++i)
    {
        mypool.AddTask([=]() {
    RandVec2(&ivec2[i]);
});

    }
    mylat.wait();
    end.now();
    std::cout << tulun::diffMicro(end, begin) << std::endl;
    std::cout << "\n===================================================\n"
              << std::endl;
}

int Add(int a, int b)
{
    return a + b;
}

void func(int i)
{
    std::cout << "func" << i << std::endl;
    ++ac;
}

void Add1()
{
    for (int i = 0; i < 2000; ++i)
    {
        mypool.AddTask(std::bind(func, i));
    }
}

void Add2()
{
    for (int i = 2000; i < 4000; ++i)
    {
        mypool.AddTask(std::bind(func, i));
    }
}

void Add3()
{
    for (int i = 4000; i < 6000; ++i)
    {
        mypool.AddTask(std::bind(func, i));
    }
}

void Add4()
{
    for (int i = 6000; i < 8000; ++i)
    {
        mypool.AddTask(std::bind(func, i));
    }
}

void Add5()
{
    for (int i = 8000; i < 10000; ++i)
    {
        mypool.AddTask(std::bind(func, i));
    }
}

int main()
{
    std::cout << "=== 线程池压力测试 ===" << std::endl;
    std::cout << "硬件线程数: " << std::thread::hardware_concurrency() << std::endl;

    std::thread tha(Add1), thb(Add2), thc(Add3), thd(Add4), the(Add5);
    tha.join();
    thb.join();
    thc.join();
    thd.join();
    the.join();

    std::this_thread::yield();
    // 等待所有任务完成
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "ac: " << ac.load() << " (expected: 10000)" << std::endl;

    std::cout << "\n=== submit 功能测试 ===" << std::endl;
    for (int i = 0; i < 10; ++i)
    {
        auto result = mypool.submit(Add, i, i * 2);
        std::cout << "submit Add(" << i << ", " << i * 2 << ") = " << result.get() << std::endl;
    }

    return 0;
}
