// TimerManage 已被 TimerManager (Timer.hpp) 替代
// 新实现是跨平台的（std::chrono + 优先队列），不依赖 Linux timerfd/epoll
// 此文件仅保留向前兼容的类型别名

#include "Timer.hpp"

#ifndef TIMER_MANAGE_HPP
#define TIMER_MANAGE_HPP

namespace tulun
{
    // 向前兼容：TimerManage 现在是 TimerManager 的别名
    using TimerManage = TimerManager;

} // namespace tulun

#endif
