# Cpp-ThreadPool —— C++ 线程池库

一个基于 **C++20** 的高性能线程池库，提供了多种线程池实现和定时任务调度能力。

## 功能特性

### 三种线程池

| 线程池 | 说明 |
|--------|------|
| **FixedThreadPool** | 固定线程数 + 共享任务队列，队列满时采用 Caller Runs 策略形成自然背压 |
| **WorkStealingPool** | 工作窃取线程池，每个线程拥有独立任务桶，空闲线程自动从其他桶窃取任务 |
| **CachedThreadPool** | 缓存式线程池，核心线程常驻，额外线程按需创建，空闲超时自动回收 |

### 定时任务

- **Timer** —— 单次/重复定时任务
- **TimerManage** —— 定时任务管理器，支持多个定时任务的统一调度

### 同步队列

提供 4 种同步队列实现（`SyncQueue_1` ~ `SyncQueue_4`），适配不同的线程池策略。

## 项目结构

```
Cpp-ThreadPool/
├── threadpool/
│   ├── include/          # 头文件
│   │   ├── FixedThreadPool.hpp
│   │   ├── WorkStealingPool.hpp
│   │   ├── CachedThreadPool.hpp
│   │   ├── Timer.hpp
│   │   ├── TimerManage.hpp
│   │   ├── SyncQueue_1.hpp ~ SyncQueue_4.hpp
│   │   ├── SyncQueueBase.hpp
│   │   ├── Timestamp.hpp
│   │   └── LogCommon.hpp
│   └── src/              # 源文件
│       └── Timestamp.cpp
├── example/              # 测试与示例
│   ├── threadpoolTest.cpp    # 完整功能测试
│   ├── benchmark.cpp         # 性能基准测试
│   ├── stress_test.cpp       # 压力测试
│   ├── timer_test.cpp        # 定时器测试
│   └── ...
├── CMakeLists.txt
└── .gitignore
```

## 构建运行

### 环境要求

- **编译器**：GCC 11+（需支持 C++20）
- **构建工具**：CMake 3.0+

### 编译

```bash
cd Cpp-ThreadPool
mkdir -p build && cd build
cmake .. && make
```

编译产物输出到 `bin/` 目录。

### 运行测试

```bash
./bin/threadpool       # 综合测试
./bin/benchmark        # 性能测试
./bin/stress_test      # 压力测试
./bin/timer_test       # 定时器测试
```

## 快速使用

```cpp
#include "FixedThreadPool.hpp"

// 创建固定大小线程池
tulun::FixedThreadPool pool(4);

// 提交普通任务
pool.submit([]() {
    // 你的任务代码
});

// 提交带返回值的任务
auto future = pool.submit([](int a, int b) -> int {
    return a + b;
}, 10, 20);
int result = future.get();  // 30
```

## 许可证

MIT License
