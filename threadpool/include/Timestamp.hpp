#include <cstdint>
#include <string>
#include <ctime>

#ifndef TIMESTAMP_HPP
#define TIMESTAMP_HPP
namespace tulun
{
    class Timestamp
    {
    private:
        uint64_t micro_; // 微秒 // 1970/01/01 08:0:0 ----- 2026/01/27 14:34:45.xxxx
    public:
        Timestamp(uint64_t ms = 0);
        ~Timestamp();

        void swap(Timestamp &te);
        std::string toString() const;
        std::string toFormattedString(bool showms = true) const;
        std::string toFormattedFile() const;

        bool valid() const;

        time_t getSecond() const;
        uint64_t getMills() const;
        uint64_t getMicro() const;
        const Timestamp &now();

        explicit operator uint64_t() const;

    public:
        static Timestamp Now();
        static Timestamp Invalid();
        static const int kMicS = 1000 * 1000; // s => mics;
        static const int kMilS = 1000;        // s => mill
    };

    inline time_t diffSecond(const Timestamp &a, const Timestamp &b)
    {
        return a.getSecond() - b.getSecond();
    }
    inline uint64_t diffMills(const Timestamp &a, const Timestamp &b)
    {
        return a.getMills() - b.getMills();
    }
    inline uint64_t diffMicro(const Timestamp &a, const Timestamp &b)
    {
        return a.getMicro() - b.getMicro();
    }

    inline Timestamp addTimeSecond(const Timestamp &a, time_t sec)
    {
        return Timestamp(a.getMicro() + sec * tulun::Timestamp::kMicS);
    }
    inline Timestamp addTimeMills(const Timestamp &a, uint64_t mills)
    {
        return Timestamp(a.getMicro() + mills * tulun::Timestamp::kMilS);
    }
    inline Timestamp addTimeMicors(const Timestamp &a, uint64_t micro)
    {
        return Timestamp(a.getMicro() + micro);
    }

} // namespace tulun
#endif
