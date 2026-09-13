#include "Timestamp.hpp"

#include <ctime>
#include <sys/time.h>
#include <cstdio>
#include <utility>

namespace tulun
{
    namespace
    {
        constexpr int kSmallBuffSize = 128;
    }

    Timestamp::Timestamp(uint64_t ms) : micro_(ms)
    {
    }
    Timestamp::~Timestamp()
    {
    }

    void Timestamp::swap(Timestamp &te)
    {
        std::swap(this->micro_, te.micro_);
    }
    std::string Timestamp::toString() const
    {
        char buff[kSmallBuffSize] = {};
        time_t ss = micro_ / kMicS;
        time_t ms = micro_ % kMicS;
        std::sprintf(buff, "%lu.%lu", ss, ms);
        return buff;
    }
    std::string Timestamp::toFormattedString(bool showms) const
    {
        char buff[kSmallBuffSize] = {};
        time_t ss = micro_ / kMicS;
        time_t ms = micro_ % kMicS;

        struct tm dtm = {};
        localtime_r(&ss, &dtm);
        int pos = std::sprintf(buff, "%04d/%02d/%02d-%02d:%02d:%02d",
                               dtm.tm_year + 1900,
                               dtm.tm_mon + 1,
                               dtm.tm_mday,
                               dtm.tm_hour,
                               dtm.tm_min,
                               dtm.tm_sec);
        if (showms)
        {
            std::sprintf(buff + pos, ".%ldZ", ms);
        }
        return buff;
    }
    std::string Timestamp::toFormattedFile() const
    {
        char buff[kSmallBuffSize] = {};
        time_t ss = micro_ / kMicS;
        time_t ms = micro_ % kMicS;

        struct tm dtm = {};
        localtime_r(&ss, &dtm);
        int pos = std::sprintf(buff, "%04d%02d%02d_%02d%02d%02d",
                               dtm.tm_year + 1900,
                               dtm.tm_mon + 1,
                               dtm.tm_mday,
                               dtm.tm_hour,
                               dtm.tm_min,
                               dtm.tm_sec);
        std::sprintf(buff + pos, ".%ldZ", ms);
        return buff;
    }

    bool Timestamp::valid() const
    {
        return micro_ > 0;
    }

    time_t Timestamp::getSecond() const
    {
        return micro_ / kMicS;
    }
    uint64_t Timestamp::getMills() const
    {
        return micro_ / kMilS;
    }
    uint64_t Timestamp::getMicro() const
    {
        return micro_;
    }

    const Timestamp &Timestamp::now()
    {
        *this = Timestamp::Now();
        return *this;
    }

    Timestamp::operator uint64_t() const
    {
        return micro_;
    }
    Timestamp Timestamp::Now()
    {
        struct timeval tv;
        gettimeofday(&tv, nullptr);
        uint64_t seconds = tv.tv_sec;
        return Timestamp(seconds * kMicS + tv.tv_usec);
    }

    Timestamp Timestamp::Invalid()
    {
        return Timestamp(0);
    }

} // namespace tulun
