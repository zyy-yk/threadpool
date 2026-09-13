#ifndef LOG_COMMON_HPP
#define LOG_COMMON_HPP
namespace tulun
{
    enum class LOG_LEVEL
    {
        TRACE = 0,
        DEBUG,
        INFO,
        WARN,
        ERROR,
        FATAL,
        NUM_LOG_LEVEL
    };

    inline constexpr const char *LLtoStr[] =
        {
            "TRACE", // 0
            "DEBUG", // 1
            "INFO",  // 2
            "WARN",  // 3
            "ERROR", // 4
            "FATAL"  // 5
        };

    inline constexpr int SMALL_BUFF_SIZE = 128;
    inline constexpr int MEDIAN_BUFF_SIZE = 512;
    inline constexpr int LARGE_BUFF_SIZE = 1024;

} // namespace tulun

#endif
