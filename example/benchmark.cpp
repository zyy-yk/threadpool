#include <iostream>
#include <iomanip>
#include <thread>
#include <functional>
#include <atomic>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <algorithm>
#include <random>
using namespace std;

#include "FixedThreadPool.hpp"
#include "CachedThreadPool.hpp"
#include "WorkStealingPool.hpp"
#include "Timer.hpp"

class Timer_{using C=chrono::high_resolution_clock;C::time_point s;public:void start(){s=C::now();}double ms(){return chrono::duration<double,milli>(C::now()-s).count();}};
class Barrier{mutex m;condition_variable c;int n;public:Barrier(int x):n(x){}void a(){lock_guard<mutex> l(m);if(--n<=0)c.notify_all();}void w(){unique_lock<mutex> l(m);c.wait(l,[this](){return n<=0;});}};
atomic<unsigned long long> sink{0};
thread_local mt19937 rng{random_device{}()};
int primes(int lim){int c=0;for(int i=2;i<=lim;++i){bool p=true;for(int j=2;j*j<=i;++j)if(i%j==0){p=false;break;}if(p)++c;}return c;}
void rsort(int n){vector<int> v(n);for(int i=0;i<n;++i)v[i]=uniform_int_distribution<int>(0,9999)(rng);sort(v.begin(),v.end());}
#define ROW(l,ms,n,bl) {double sp=(ms>.01&&bl>.01)?bl/ms:0;cout<<left<<setw(24)<<l<<right<<fixed<<setprecision(1)<<setw(8)<<ms<<" ms"<<setw(8)<<setprecision(0)<<(ms>0?n/ms:0)<<"/ms"<<setw(7)<<setprecision(2)<<sp<<"x"<<endl;}

int main(){
    int hw=thread::hardware_concurrency();
    cout<<"\n  ╔══════════════════════════════════════════════╗\n"
          "  ║  四种线程池 性能基准 (gcc 4.9 -O2, "<<setw(2)<<hw<<"核)  ║\n"
          "  ║  每池创建一次运行三项测试                       ║\n"
          "  ╚══════════════════════════════════════════════╝\n";

    const int N1=2000, M1=200;       // random_sort
    const int N2=400,  LIM2=4000;   // submit primes
    const int N3=20000;              // micro atomic++

    // 基准线
    double bl1,bl2,bl3;
    {Timer_ t;t.start();for(int i=0;i<N1;++i)rsort(M1);bl1=t.ms();}
    {Timer_ t;t.start();int s=0;for(int i=0;i<N2;++i)s+=primes(LIM2);bl2=t.ms();sink+=s;}
    {Timer_ t;t.start();for(int i=0;i<N3;++i)sink.fetch_add(1,memory_order_relaxed);bl3=t.ms();}
    cout<<"  单线程基线: rand_sort="<<bl1<<"ms  primes="<<bl2<<"ms  atomic="<<bl3<<"ms\n";

    // ===== WorkStealingPool (一次创建) =====
    {
        cout<<"\n  *** WorkStealingPool ***\n"<<string(52,'-')<<endl;
        tulun::WorkStealingPool p(max({N1,N2,N3}),hw);

        {Barrier b(N1);Timer_ t;t.start();
            for(int i=0;i<N1;++i)p.AddTask([&](){rsort(M1);b.a();});b.w();
            ROW("random_sort",t.ms(),N1,bl1);}

        {Timer_ t;t.start();vector<future<int>> f;
            for(int i=0;i<N2;++i)f.push_back(p.submit(primes,LIM2));
            int tot=0;for(auto&fu:f)tot+=fu.get();
            ROW("submit primes",t.ms(),N2,bl2);}

        {atomic<int> c{0};Timer_ t;t.start();
            for(int i=0;i<N3;++i)p.AddTask([&c](){c.fetch_add(1,memory_order_relaxed);});
            while(c.load()<N3)this_thread::sleep_for(chrono::milliseconds(1));
            ROW("micro atomic++",t.ms(),N3,bl3);}
    }

    // ===== CachedThreadPool (一次创建) =====
    {
        cout<<"\n  *** CachedThreadPool ***\n"<<string(52,'-')<<endl;
        tulun::CachedThreadPool p(4,hw+2,60000,max({N1,N2,N3}));

        {Barrier b(N1);Timer_ t;t.start();
            for(int i=0;i<N1;++i)p.AddTask([&](){rsort(M1);b.a();});b.w();
            ROW("random_sort",t.ms(),N1,bl1);}

        {Timer_ t;t.start();vector<future<int>> f;
            for(int i=0;i<N2;++i)f.push_back(p.submit(primes,LIM2));
            int tot=0;for(auto&fu:f)tot+=fu.get();
            ROW("submit primes",t.ms(),N2,bl2);}

        {atomic<int> c{0};Timer_ t;t.start();
            for(int i=0;i<N3;++i)p.AddTask([&c](){c.fetch_add(1,memory_order_relaxed);});
            while(c.load()<N3)this_thread::sleep_for(chrono::milliseconds(1));
            ROW("micro atomic++",t.ms(),N3,bl3);}
    }

    // ===== FixedThreadPool (一次创建，必须最后) =====
    {
        cout<<"\n  *** FixedThreadPool ***\n"<<string(52,'-')<<endl;
        tulun::FixedThreadPool p(max({N1,N2,N3}),hw);

        {Barrier b(N1);Timer_ t;t.start();
            for(int i=0;i<N1;++i)p.AddTask([&](){rsort(M1);b.a();});b.w();
            ROW("random_sort",t.ms(),N1,bl1);}

        {Timer_ t;t.start();vector<future<int>> f;
            for(int i=0;i<N2;++i)f.push_back(p.submit(primes,LIM2));
            int tot=0;for(auto&fu:f)tot+=fu.get();
            ROW("submit primes",t.ms(),N2,bl2);}

        {atomic<int> c{0};Timer_ t;t.start();
            for(int i=0;i<N3;++i)p.AddTask([&c](){c.fetch_add(1,memory_order_relaxed);});
            while(c.load()<N3)this_thread::sleep_for(chrono::milliseconds(1));
            ROW("micro atomic++",t.ms(),N3,bl3);}
    }

    // ===== Timer =====
    {
        cout<<"\n  *** Timer ***\n"<<string(52,'-')<<endl;
        tulun::TimerManager tm;
        atomic<int> tc{0};
        tm.runAfter(100,[&tc](){tc++;});
        tm.runEvery(200,[&tc](){tc++;});
        this_thread::sleep_for(chrono::seconds(1));
        cout<<"  1秒内触发 "<<tc.load()<<" 次 (预期~6)  "<<(tc.load()>=5?"PASS":"CHECK")<<endl;
        tm.stop();
    }

    // ===== 手动多线程参考 =====
    {
        cout<<"\n  *** 手动多线程(参考) ***\n"<<string(52,'-')<<endl;
        {Timer_ t;t.start();vector<thread> th;vector<int> r(hw);int per=N2/hw;
            for(int ti=0;ti<hw;++ti)th.emplace_back([ti,per,LIM2,&r,hw](){
                int s=0,e=(ti==hw-1)?N2:(ti+1)*per;for(int i=ti*per;i<e;++i)s+=primes(LIM2);r[ti]=s;});
            for(auto&thr:th)thr.join();
            ROW("submit primes",t.ms(),N2,bl2);}
    }

    cout<<"\n  ========== 全部测试完成 ==========\n"<<endl;
    return 0;
}
